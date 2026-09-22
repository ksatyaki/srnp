/*
  client.cpp - Implementation of classes and functions in client.h

  Copyright (C) 2015  Chittaranjan Srinivas Swaminathan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include <srnp/client.h>
#include <srnp/msgs/codec.h>
#include <srnp/srnp_print.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>

namespace srnp {
namespace {

/// The value a meta-pair holds before it points anywhere.
constexpr std::string_view kNullMeta = "(META -1 NULL)";

/// How many frames to hold for a component that hasn't connected yet.
constexpr std::size_t kMaxPendingFrames = 1024;

/// Reads a whole string as an int. Returns nullopt for anything else.
std::optional<int> parseInt(std::string_view text) {
  int value = 0;
  const auto* end = text.data() + text.size();
  const auto [stop, error] = std::from_chars(text.data(), end, value);
  if (error != std::errc{} || stop != end) return std::nullopt;
  return value;
}

/// Pulls <owner, key> out of a meta-pair value, or nullopt if it isn't one.
std::optional<PairKey> parseMetaValue(std::string_view value) {
  const auto parts = extractStrings(value);
  if (parts.size() != 3 || parts[0] != "META") return std::nullopt;

  const auto owner = parseInt(parts[1]);
  if (!owner) return std::nullopt;
  return PairKey{*owner, parts[2]};
}

}  // namespace

std::vector<std::string> extractStrings(std::string_view text) {
  std::vector<std::string> words;
  for (const auto part : std::views::split(text, ' ')) {
    std::string_view word(part.begin(), part.end());
    // Strip the parentheses that wrap a meta value.
    while (!word.empty() && (word.front() == '(' || word.front() == ')')) word.remove_prefix(1);
    while (!word.empty() && (word.back() == '(' || word.back() == ')')) word.remove_suffix(1);
    if (!word.empty()) words.emplace_back(word);
  }
  return words;
}

/** CLIENT SESSION **/

ClientSession::ClientSession(asio::io_context& io, std::string host, std::string port,
                             Client& client, int endpoint_owner_id)
    : io_(io),
      strand_(asio::make_strand(io)),
      host_(std::move(host)),
      port_(std::move(port)),
      client_(client),
      endpoint_owner_id_(endpoint_owner_id) {}

asio::awaitable<bool> ClientSession::connect() {
  tcp::resolver resolver(io_);
  tcp::socket socket(io_);

  auto [resolve_error, endpoints] =
      co_await resolver.async_resolve(host_, port_, asio::as_tuple(asio::use_awaitable));
  if (resolve_error) {
    SRNP_DEBUG("cannot resolve {}:{}: {}", host_, port_, resolve_error.message());
    co_return false;
  }

  auto [connect_error, _] =
      co_await asio::async_connect(socket, endpoints, asio::as_tuple(asio::use_awaitable));
  if (connect_error) {
    SRNP_DEBUG("cannot reach {}:{}: {}", host_, port_, connect_error.message());
    co_return false;
  }

  auto channel = std::make_shared<FrameChannel>(std::move(socket), strand_);

  std::deque<Pending> waiting;
  {
    std::lock_guard lock(channel_mutex_);
    // close() may have run while we were connecting. Adopting the channel
    // now would leave it open with nobody to shut it down.
    if (closing_.load(std::memory_order_relaxed)) {
      channel->close();
      co_return false;
    }
    channel_ = channel;
    waiting.swap(pending_);
  }

  // Anything queued while we were connecting goes out first, in order.
  for (auto& queued : waiting)
    channel->send(std::move(queued.bytes), std::move(queued.coalesce_key));
  co_return true;
}

asio::awaitable<void> ClientSession::run() {
  auto self = shared_from_this();
  asio::steady_timer retry(io_);

  while (!closing_.load(std::memory_order_relaxed)) {
    if (!co_await connect()) {
      retry.expires_after(kReconnectDelay);
      co_await retry.async_wait(asio::as_tuple(asio::use_awaitable));
      continue;
    }

    SRNP_DEBUG("connected to {}:{}", host_, port_);
    // Tell the component what we want from it, then idle. We never read
    // from this socket: its owner pushes to our server, not down this one.
    sendSubscriptionsFor(endpoint_owner_id_);
    co_return;
  }
}

void ClientSession::sendSubscriptionsFor(int owner) {
  for (const auto& record : client_.subscriptionsFor(owner)) {
    Subscription message;
    message.key = record.key;
    // A wildcard subscription is addressed to whoever we are talking to.
    message.owner_id = record.owner == kAnyOwner ? owner : record.owner;
    message.subscriber = client_.ownerId();
    message.registering = true;
    send(wire::frameOf(wire::MessageType::Subscription, message));
  }
}

void ClientSession::send(std::vector<std::byte> frame,
                         std::optional<PairKey> coalesce_key) {
  if (closing_.load(std::memory_order_relaxed)) return;

  // One critical section throughout: checking the channel and queueing in
  // two steps would let a connection complete in between and strand the
  // frame in a queue nobody flushes again.
  std::lock_guard lock(channel_mutex_);

  if (channel_) {
    channel_->send(std::move(frame), std::move(coalesce_key));
    return;
  }

  // Not connected yet. Hold onto it, but don't grow without limit if the
  // component never comes up.
  if (pending_.size() >= kMaxPendingFrames) {
    SRNP_WARN("{}:{} is still unreachable; dropping the oldest queued frame", host_, port_);
    pending_.pop_front();
  }
  pending_.push_back(Pending{std::move(frame), std::move(coalesce_key)});
}

void ClientSession::close() {
  closing_.store(true, std::memory_order_relaxed);

  FrameChannelPtr channel;
  {
    std::lock_guard lock(channel_mutex_);
    channel = std::move(channel_);
    pending_.clear();
  }
  if (channel) channel->close();
}

/** CLIENT **/

Client::Client(asio::io_context& io, PairSpace& pair_space)
    : io_(io), pair_space_(pair_space), callback_strand_(asio::make_strand(io)) {}

Client::~Client() { close(); }

void Client::close() {
  std::vector<ClientSessionPtr> sessions;
  {
    std::lock_guard lock(state_mutex_);
    for (auto& [owner, session] : sessions_) sessions.push_back(session);
    sessions_.clear();
  }

  for (auto& session : sessions) session->close();
}

bool Client::waitUntilReady(std::chrono::milliseconds timeout) {
  std::unique_lock lock(ready_mutex_);
  return ready_changed_.wait_for(lock, timeout, [this] { return ready(); });
}

void Client::onWelcome(const MasterMessage& welcome) {
  owner_id_.store(welcome.owner, std::memory_order_relaxed);
  for (const auto& component : welcome.all_components) addSession(component);

  {
    std::lock_guard lock(ready_mutex_);
    ready_.store(true, std::memory_order_release);
  }
  ready_changed_.notify_all();
}

void Client::onComponentUpdate(const UpdateComponents& update) {
  if (update.operation == UpdateComponents::Operation::Add) {
    SRNP_DEBUG("component {} joined at {}:{}", update.component.owner, update.component.ip,
               update.component.port);
    addSession(update.component);
  } else {
    SRNP_DEBUG("component {} left", update.component.owner);
    removeSession(update.component.owner);
  }
}

void Client::fanOutPair(const Pair& pair, std::span<const int> subscribers) {
  const auto payload = wire::frameOf(wire::MessageType::PairUpdate, pair);
  // A pair update is last-value-wins, so a subscriber that falls behind gets
  // this one in place of whatever it had not read yet for the same key.
  const PairKey key{pair.getOwner(), pair.getKey()};
  for (const int subscriber : subscribers) {
    if (auto session = sessionFor(subscriber))
      session->send(payload, key);
    else
      SRNP_WARN("subscriber {} is not connected", subscriber);
  }
}

void Client::fanOutRemoval(const RemovePairRequest& request, std::span<const int> subscribers) {
  const auto payload = wire::frameOf(wire::MessageType::PairRemoved, request);
  for (const int subscriber : subscribers) {
    if (auto session = sessionFor(subscriber))
      session->send(payload);
    else
      SRNP_WARN("subscriber {} is not connected", subscriber);
  }
}

void Client::notifyLocal(Pair::ConstPtr snapshot,
                         std::vector<std::shared_ptr<const Pair::CallbackFunction>> callbacks) {
  if (callbacks.empty()) return;

  // Posted rather than called here: setPair must not run user code on its
  // caller's thread, and one strand keeps two rapid publishes in order.
  asio::post(callback_strand_,
             [snapshot = std::move(snapshot), callbacks = std::move(callbacks)] {
               for (const auto& callback : callbacks) (*callback)(snapshot);
             });
}

void Client::addSession(const ComponentInfo& component) {
  auto session =
      std::make_shared<ClientSession>(io_, component.ip, component.port, *this, component.owner);
  {
    std::lock_guard lock(state_mutex_);
    // A component reusing an owner id replaces the stale session.
    sessions_.insert_or_assign(component.owner, session);
    components_.insert_or_assign(component.owner, component);
  }
  asio::co_spawn(session->strand(), session->run(), asio::detached);
}

void Client::removeSession(int owner) {
  ClientSessionPtr session;
  {
    std::lock_guard lock(state_mutex_);
    if (const auto it = sessions_.find(owner); it != sessions_.end()) {
      session = it->second;
      sessions_.erase(it);
    }
    components_.erase(owner);
  }
  if (session) session->close();
}

std::map<int, ComponentInfo> Client::components() const {
  std::lock_guard lock(state_mutex_);
  return components_;
}

ClientSessionPtr Client::sessionFor(int owner) const {
  std::lock_guard lock(state_mutex_);
  const auto it = sessions_.find(owner);
  return it == sessions_.end() ? nullptr : it->second;
}

std::vector<ClientSessionPtr> Client::allSessions() const {
  std::lock_guard lock(state_mutex_);
  std::vector<ClientSessionPtr> sessions;
  sessions.reserve(sessions_.size());
  for (const auto& [owner, session] : sessions_) sessions.push_back(session);
  return sessions;
}

std::vector<Client::SubscriptionRecord> Client::subscriptionsFor(int owner) const {
  std::lock_guard lock(state_mutex_);
  std::vector<SubscriptionRecord> matching;
  for (const auto& [handle, record] : subscriptions_)
    if (record.owner == kAnyOwner || record.owner == owner) matching.push_back(record);
  return matching;
}

bool Client::setPair(std::string_view key, std::string_view value, Pair::Type type) {
  if (!ready()) return false;

  const Pair incoming(ownerId(), std::string(key), std::string(value), type);

  Pair::ConstPtr snapshot;
  std::vector<std::shared_ptr<const Pair::CallbackFunction>> callbacks;
  {
    // Applying, reading the subscriber list and queueing the frames all
    // happen under this one lock. Split it and two threads publishing the
    // same key can queue their frames in the opposite order to the one they
    // applied in, and the subscriber ends up holding the older value.
    std::lock_guard lock(pair_space_.mutex);
    const PairEntry& entry = pair_space_.addPair(incoming);
    if (!entry.subscribers.empty()) fanOutPair(entry.pair, entry.subscribers);
    snapshot = std::make_shared<const Pair>(entry.pair);

    callbacks = pair_space_.universalCallbacks();
    callbacks.reserve(callbacks.size() + entry.callbacks.size());
    for (const auto& [handle, callback] : entry.callbacks) callbacks.push_back(callback);
  }

  notifyLocal(std::move(snapshot), std::move(callbacks));
  return true;
}

bool Client::setRemotePair(int owner, std::string_view key, std::string_view value,
                           Pair::Type type) {
  auto session = sessionFor(owner);
  if (!session) {
    SRNP_WARN("cannot set a pair on {}: not connected", owner);
    return false;
  }

  const Pair pair(owner, std::string(key), std::string(value), type);
  session->send(wire::frameOf(wire::MessageType::Pair, pair));
  return true;
}

bool Client::removePair(std::string_view key) {
  if (!ready()) return false;

  const RemovePairRequest request{ownerId(), std::string(key)};
  {
    std::lock_guard lock(pair_space_.mutex);
    const PairEntry* entry = pair_space_.find(request.owner, request.key);
    // Removing a key that was never published succeeds and does nothing.
    // Documented behaviour, not an oversight: see docs/user/api-reference.md.
    if (entry == nullptr) {
      SRNP_DEBUG("nothing to remove for [{}] {}", request.owner, request.key);
      return true;
    }
    // Read the subscribers before the removal: that is what takes the list away.
    const std::vector<int> subscribers = entry->subscribers;
    pair_space_.removePair(request.owner, request.key);
    if (!subscribers.empty()) fanOutRemoval(request, subscribers);
  }
  return true;
}

bool Client::removeRemotePair(int owner, std::string_view key) {
  if (owner == ownerId()) return removePair(key);

  auto session = sessionFor(owner);
  if (!session) {
    SRNP_WARN("cannot remove a pair on {}: not connected", owner);
    return false;
  }

  const RemovePairRequest request{owner, std::string(key)};
  session->send(wire::frameOf(wire::MessageType::RemovePair, request));
  return true;
}

std::optional<Pair> Client::getPair(int owner, std::string_view key) {
  std::lock_guard lock(pair_space_.mutex);
  const auto pair = pair_space_.copyOf(owner, key);

  // A Type::Invalid entry is a placeholder holding a subscription or a
  // callback for a key that has no value: either never published, or
  // removed. Either way there is nothing to hand back.
  if (pair && pair->getType() == Pair::Type::Invalid) return std::nullopt;
  return pair;
}

std::optional<Pair> Client::getPairIndirectly(int metaowner, std::string_view metakey) {
  const auto metapair = getPair(metaowner, metakey);
  if (!metapair) {
    SRNP_WARN("no meta-pair [{}] {}. Did you subscribe to it?", metaowner, metakey);
    return std::nullopt;
  }

  const auto target = parseMetaValue(metapair->getValue());
  if (!target) {
    SRNP_WARN("[{}] {} is not a meta-pair", metaowner, metakey);
    return std::nullopt;
  }
  return getPair(target->owner, target->key);
}

bool Client::setPairIndirectly(int metaowner, std::string_view metakey, std::string_view value) {
  const auto metapair = getPair(metaowner, metakey);
  if (!metapair) {
    SRNP_WARN("no meta-pair [{}] {}. Did you subscribe to it?", metaowner, metakey);
    return false;
  }

  const auto target = parseMetaValue(metapair->getValue());
  if (!target) {
    SRNP_WARN("[{}] {} is not a meta-pair", metaowner, metakey);
    return false;
  }

  if (target->owner == ownerId()) return setPair(target->key, value);
  return setRemotePair(target->owner, target->key, value);
}

bool Client::setMetaPair(int meta_owner, std::string_view meta_key, int owner,
                         std::string_view key) {
  const auto value = std::format("(META {} {})", owner, key);
  if (meta_owner == ownerId()) return setPair(meta_key, value, Pair::Type::String);
  return setRemotePair(meta_owner, meta_key, value, Pair::Type::String);
}

bool Client::initMetaPair(int meta_owner, std::string_view meta_key) {
  if (meta_owner == ownerId()) return setPair(meta_key, kNullMeta, Pair::Type::Meta);
  return setRemotePair(meta_owner, meta_key, kNullMeta, Pair::Type::Meta);
}

CallbackHandle Client::registerCallback(int owner, std::string_view key,
                                        Pair::CallbackFunction callback_fn) {
  std::lock_guard lock(pair_space_.mutex);

  if (key == kWildcardKey) return pair_space_.addCallbackToAll(std::move(callback_fn));
  return pair_space_.addCallback(owner, key, std::move(callback_fn));
}

void Client::cancelCallback(CallbackHandle handle) {
  std::lock_guard lock(pair_space_.mutex);
  pair_space_.removeCallback(handle);
}

bool Client::sendSubscription(int owner, std::string_view key, bool registering) {
  Subscription message;
  message.key = std::string(key);
  message.subscriber = ownerId();
  message.registering = registering;

  if (owner != kAnyOwner) {
    auto session = sessionFor(owner);
    if (!session) return false;  // Replayed when that component connects.
    message.owner_id = owner;
    session->send(wire::frameOf(wire::MessageType::Subscription, message));
    return true;
  }

  // A wildcard subscription goes to everyone we know about, addressed to each.
  for (const auto& session : allSessions()) {
    message.owner_id = session->endpointOwner();
    session->send(wire::frameOf(wire::MessageType::Subscription, message));
  }
  return true;
}

SubscriptionHandle Client::registerSubscription(int owner, std::string_view key) {
  SubscriptionHandle handle = kInvalidSubscriptionHandle;
  {
    std::lock_guard lock(state_mutex_);
    const auto duplicate = std::ranges::any_of(subscriptions_, [&](const auto& entry) {
      return entry.second.owner == owner && entry.second.key == key;
    });
    if (duplicate) {
      SRNP_WARN("already subscribed to [{}] {}", owner, key);
      return kInvalidSubscriptionHandle;
    }

    handle = ++next_subscription_handle_;
    subscriptions_.emplace(handle, SubscriptionRecord{owner, std::string(key)});
  }

  sendSubscription(owner, key, true);
  return handle;
}

SubscriptionHandle Client::registerSubscription(std::string_view key) {
  return registerSubscription(kAnyOwner, key);
}

void Client::cancelSubscription(int owner, std::string_view key) {
  {
    std::lock_guard lock(state_mutex_);
    const auto it = std::ranges::find_if(subscriptions_, [&](const auto& entry) {
      return entry.second.owner == owner && entry.second.key == key;
    });
    if (it == subscriptions_.end()) {
      SRNP_WARN("not subscribed to [{}] {}", owner, key);
      return;
    }
    subscriptions_.erase(it);
  }

  sendSubscription(owner, key, false);
}

void Client::cancelSubscription(std::string_view key) { cancelSubscription(kAnyOwner, key); }

void Client::cancelSubscription(SubscriptionHandle handle) {
  SubscriptionRecord record;
  {
    std::lock_guard lock(state_mutex_);
    const auto it = subscriptions_.find(handle);
    if (it == subscriptions_.end()) {
      SRNP_WARN("no subscription under handle {}", handle);
      return;
    }
    record = it->second;
    subscriptions_.erase(it);
  }

  sendSubscription(record.owner, record.key, false);
}

}  // namespace srnp

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
                             Client& client, bool is_our_own_server, int endpoint_owner_id)
    : io_(io),
      strand_(asio::make_strand(io)),
      host_(std::move(host)),
      port_(std::move(port)),
      client_(client),
      is_our_own_server_(is_our_own_server),
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

  std::deque<std::vector<std::byte>> waiting;
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
  for (auto& frame : waiting) channel->send(std::move(frame));
  co_return true;
}

asio::awaitable<void> ClientSession::run() {
  auto self = shared_from_this();
  asio::steady_timer retry(io_);

  while (!closing_.load(std::memory_order_relaxed)) {
    if (!co_await connect()) {
      // Our own server is in this process, so failing to reach it is fatal
      // rather than something to retry.
      if (is_our_own_server_) {
        SRNP_FATAL("cannot connect to our own server on {}:{}", host_, port_);
        co_return;
      }
      retry.expires_after(kReconnectDelay);
      co_await retry.async_wait(asio::as_tuple(asio::use_awaitable));
      continue;
    }

    SRNP_DEBUG("connected to {}:{}", host_, port_);

    if (!is_our_own_server_) {
      // A component we only push to. Tell it what we want, then idle.
      sendSubscriptionsFor(endpoint_owner_id_);
      co_return;
    }

    // Tell our server which connection we are; other components' clients
    // connect to the same acceptor.
    send(wire::frame(wire::MessageType::AttachClient));

    co_await readLoop();
    co_return;
  }
}

asio::awaitable<void> ClientSession::readLoop() {
  FrameChannelPtr channel;
  {
    std::lock_guard lock(channel_mutex_);
    channel = channel_;
  }
  if (!channel) co_return;  // Closed before we got going.

  try {
    for (;;) {
      const auto frame = co_await channel->read();
      switch (frame.header.type) {
        case wire::MessageType::MasterMessage: handleMasterMessage(frame); break;
        case wire::MessageType::UpdateComponents: handleUpdateComponents(frame); break;
        case wire::MessageType::PairUpdate:
        case wire::MessageType::PairUpdateOne: handlePairUpdate(frame); break;
        default:
          throw wire::DecodeError("a client cannot handle this message type");
      }
    }
  } catch (const std::exception& e) {
    if (!closing_.load(std::memory_order_relaxed))
      SRNP_ERROR("lost the connection to our own server: {}", e.what());
  }
}

void ClientSession::handleMasterMessage(const Frame& frame) {
  const auto message = decodePayload<MasterMessage>(frame.bytes());
  endpoint_owner_id_ = message.owner;
  client_.onMasterMessage(message.owner, message.all_components);
}

void ClientSession::handleUpdateComponents(const Frame& frame) {
  const auto message = decodePayload<UpdateComponents>(frame.bytes());

  if (message.operation == UpdateComponents::Operation::Add) {
    SRNP_DEBUG("component {} joined at {}:{}", message.component.owner, message.component.ip,
               message.component.port);
    client_.addSession(message.component);
  } else {
    SRNP_DEBUG("component {} left", message.component.owner);
    client_.removeSession(message.component.owner);
  }
}

void ClientSession::handlePairUpdate(const Frame& frame) {
  auto pair = client_.pair_queue_.updates.pop();
  if (!pair) {
    SRNP_ERROR("got a pair update notification with nothing queued behind it");
    return;
  }

  const int only = frame.header.type == wire::MessageType::PairUpdateOne
                       ? frame.header.subscriber
                       : kAnyOwner;
  forwardPairUpdate(*pair, only);
}

void ClientSession::forwardPairUpdate(const Pair& pair, int only_subscriber) {
  const auto payload = wire::frameOf(wire::MessageType::PairUpdate, pair);

  if (only_subscriber != kAnyOwner) {
    if (auto session = client_.sessionFor(only_subscriber))
      session->send(payload);
    else
      SRNP_WARN("subscriber {} is not connected", only_subscriber);
    return;
  }

  for (const int subscriber : pair.subscribers_) {
    if (auto session = client_.sessionFor(subscriber))
      session->send(payload);
    else
      SRNP_WARN("subscriber {} is not connected", subscriber);
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

void ClientSession::send(std::vector<std::byte> frame) {
  if (closing_.load(std::memory_order_relaxed)) return;

  // One critical section throughout: checking the channel and queueing in
  // two steps would let a connection complete in between and strand the
  // frame in a queue nobody flushes again.
  std::lock_guard lock(channel_mutex_);

  if (channel_) {
    channel_->send(std::move(frame));
    return;
  }

  // Not connected yet. Hold onto it, but don't grow without limit if the
  // component never comes up.
  if (pending_.size() >= kMaxPendingFrames) {
    SRNP_WARN("{}:{} is still unreachable; dropping the oldest queued frame", host_, port_);
    pending_.pop_front();
  }
  pending_.push_back(std::move(frame));
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

Client::Client(asio::io_context& io, std::string our_server_ip, std::string our_server_port,
               PairSpace& pair_space, PairQueue& pair_queue)
    : io_(io), pair_space_(pair_space), pair_queue_(pair_queue) {
  // ready_ is already false, so the session below can flip it the moment
  // the master message arrives without the constructor racing it back.
  my_server_session_ = std::make_shared<ClientSession>(io_, std::move(our_server_ip),
                                                       std::move(our_server_port), *this, true,
                                                       kAnyOwner);
  asio::co_spawn(my_server_session_->strand(), my_server_session_->run(), asio::detached);
}

Client::~Client() { close(); }

void Client::close() {
  std::vector<ClientSessionPtr> sessions;
  {
    std::lock_guard lock(state_mutex_);
    for (auto& [owner, session] : sessions_) sessions.push_back(session);
    sessions_.clear();
  }

  for (auto& session : sessions) session->close();
  if (my_server_session_) my_server_session_->close();
}

bool Client::waitUntilReady(std::chrono::milliseconds timeout) {
  std::unique_lock lock(ready_mutex_);
  return ready_changed_.wait_for(lock, timeout, [this] { return ready(); });
}

void Client::onMasterMessage(int owner_id, const std::vector<ComponentInfo>& components) {
  owner_id_.store(owner_id, std::memory_order_relaxed);
  for (const auto& component : components) addSession(component);

  {
    std::lock_guard lock(ready_mutex_);
    ready_.store(true, std::memory_order_release);
  }
  ready_changed_.notify_all();
}

void Client::addSession(const ComponentInfo& component) {
  auto session = std::make_shared<ClientSession>(io_, component.ip, component.port, *this, false,
                                                 component.owner);
  {
    std::lock_guard lock(state_mutex_);
    // A component reusing an owner id replaces the stale session.
    sessions_.insert_or_assign(component.owner, session);
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
  }
  if (session) session->close();
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
  if (!my_server_session_) return false;

  Pair pair(ownerId(), std::string(key), std::string(value), type);

  // The pair goes through the queue rather than the socket; the empty frame
  // just tells our server there is one waiting.
  auto guard = pair_queue_.outgoing.lock();
  pair_queue_.outgoing.push(std::move(pair));
  my_server_session_->send(wire::frame(wire::MessageType::PairNoCopy));
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

std::optional<Pair> Client::getPair(int owner, std::string_view key) {
  std::lock_guard lock(pair_space_.mutex);
  return pair_space_.copyOf(owner, key);
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

  if (key == kWildcardKey) {
    pair_space_.addCallbackToAll(std::move(callback_fn));
    return kInvalidCallbackHandle;
  }
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

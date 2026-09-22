/*
  server.cpp - Implementation of the server side.

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

#include <srnp/msgs/codec.h>
#include <srnp/server.h>
#include <srnp/srnp_print.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <utility>

namespace srnp {

/** SERVER SESSION **/

ServerSession::ServerSession(tcp::socket socket, Strand strand, Server& server)
    : channel_(std::make_shared<FrameChannel>(std::move(socket), std::move(strand))),
      server_(server) {}

asio::awaitable<void> ServerSession::run() {
  // Keeps the session alive for as long as the loop runs, which replaces
  // the old hand-managed delete.
  auto self = shared_from_this();

  try {
    for (;;) handle(co_await channel_->read());
  } catch (const wire::DecodeError& e) {
    SRNP_WARN("dropping a connection that sent a bad frame: {}", e.what());
  } catch (const std::exception& e) {
    SRNP_DEBUG("connection closed: {}", e.what());
  }

  channel_->close();
}

void ServerSession::handle(const Frame& frame) {
  switch (frame.header.type) {
    case wire::MessageType::Pair: handleIncomingPair(frame); break;
    case wire::MessageType::PairUpdate: handlePairUpdate(frame); break;
    case wire::MessageType::Subscription: handleSubscription(frame); break;
    case wire::MessageType::RemovePair: handleRemovePair(frame); break;
    case wire::MessageType::PairRemoved: handlePairRemoved(frame); break;
    default:
      throw wire::DecodeError("a server cannot handle this message type");
  }
}

Pair::ConstPtr ServerSession::applyAndNotify(const Pair& pair) {
  Pair::ConstPtr snapshot;
  std::vector<std::shared_ptr<const Pair::CallbackFunction>> callbacks;
  std::vector<int> subscribers;

  {
    std::lock_guard lock(server_.pairSpace().mutex);
    PairEntry& entry = server_.pairSpace().addPair(pair);
    snapshot = std::make_shared<const Pair>(entry.pair);
    subscribers = entry.subscribers;

    callbacks = server_.pairSpace().universalCallbacks();
    callbacks.reserve(callbacks.size() + entry.callbacks.size());
    for (const auto& [handle, callback] : entry.callbacks) callbacks.push_back(callback);
  }

  // Outside the lock, so a slow user callback cannot block the pair space.
  for (const auto& callback : callbacks) (*callback)(snapshot);

  if (!subscribers.empty()) server_.localClient().fanOutPair(*snapshot, subscribers);
  return snapshot;
}

void ServerSession::handleIncomingPair(const Frame& frame) {
  auto pair = decodePayload<Pair>(frame.bytes());
  // The sender addressed this to us, so we own it once it lands.
  pair.setOwner(server_.owner());
  (void)applyAndNotify(pair);
}

void ServerSession::handlePairUpdate(const Frame& frame) {
  // Someone else's pair that we subscribed to. Apply it and run callbacks,
  // but don't forward it on: we are not its owner.
  applyAndNotify(decodePayload<Pair>(frame.bytes()));
}

void ServerSession::handleSubscription(const Frame& frame) {
  const auto message = decodePayload<Subscription>(frame.bytes());
  const bool wildcard = message.key == kWildcardKey;

  if (!wildcard && message.owner_id != server_.owner()) {
    SRNP_WARN("ignoring a subscription meant for {}", message.owner_id);
    return;
  }
  if (message.subscriber == server_.owner()) {
    SRNP_WARN("ignoring a subscription from ourselves");
    return;
  }

  // Collected under the lock, sent after releasing it.
  std::vector<Pair> to_send;
  {
    std::lock_guard lock(server_.pairSpace().mutex);
    auto& space = server_.pairSpace();

    if (!message.registering) {
      if (wildcard)
        space.removeSubscriptionToAll(message.subscriber);
      else
        space.removeSubscription(message.owner_id, message.key, message.subscriber);
      return;
    }

    if (wildcard) {
      space.addSubscriptionToAll(message.subscriber);
      // Send everything we own so the new subscriber starts up to date.
      for (const auto& [key, entry] : space.getAllPairs())
        if (entry.pair.getOwner() == server_.owner() &&
            entry.pair.getType() != Pair::Type::Invalid)
          to_send.push_back(entry.pair);
    } else {
      space.addSubscription(message.owner_id, message.key, message.subscriber);
      if (const PairEntry* entry = space.find(message.owner_id, message.key);
          entry != nullptr && entry->pair.getType() != Pair::Type::Invalid) {
        to_send.push_back(entry->pair);
      }
    }
  }

  for (const auto& pair : to_send) sendPairUpdate(pair, message.subscriber);
}

void ServerSession::handleRemovePair(const Frame& frame) {
  const auto message = decodePayload<RemovePairRequest>(frame.bytes());
  if (message.owner != server_.owner()) {
    SRNP_WARN("ignoring a removal meant for {}", message.owner);
    return;
  }

  // The subscribers have to be read before the pair goes, since the removal
  // is what takes the list away.
  std::vector<int> subscribers;
  {
    std::lock_guard lock(server_.pairSpace().mutex);
    const PairEntry* entry = server_.pairSpace().find(message.owner, message.key);
    if (entry == nullptr) {
      SRNP_WARN("cannot remove [{}] {}: no such pair", message.owner, message.key);
      return;
    }
    subscribers = entry->subscribers;
    server_.pairSpace().removePair(message.owner, message.key);
  }

  if (subscribers.empty()) return;
  server_.localClient().fanOutRemoval(message, subscribers);
}

void ServerSession::handlePairRemoved(const Frame& frame) {
  // The owner dropped a pair we subscribed to, so drop our copy too.
  const auto message = decodePayload<RemovePairRequest>(frame.bytes());
  std::lock_guard lock(server_.pairSpace().mutex);
  server_.pairSpace().removePair(message.owner, message.key);
}

void ServerSession::sendPairUpdate(const Pair& pair, int subscriber) {
  const int one[] = {subscriber};
  server_.localClient().fanOutPair(pair, one);
}

/** MASTER LINK **/

MasterLink::MasterLink(asio::io_context& io, std::string master_ip, std::string master_port)
    : master_ip_(std::move(master_ip)), master_port_(std::move(master_port)) {
  tcp::resolver resolver(io);
  tcp::socket socket(io);
  asio::connect(socket, resolver.resolve(master_ip_, master_port_));
  channel_ = std::make_shared<FrameChannel>(std::move(socket), asio::make_strand(io));
}

MasterMessage MasterLink::registerWithMaster(unsigned short our_port, int desired_owner_id) {
  IndicatePresence presence;
  presence.port = std::to_string(our_port);
  presence.force_owner_id = desired_owner_id != kAnyOwner;
  presence.owner_id = desired_owner_id;

  const auto request = wire::frameOf(wire::MessageType::IndicatePresence, presence);
  asio::write(channel_->socket(), asio::buffer(request));

  // Registration is synchronous: nothing else can happen until we have an id.
  std::array<std::byte, wire::kHeaderSize> header_bytes{};
  asio::read(channel_->socket(), asio::buffer(header_bytes));
  const auto header = wire::decodeHeader(header_bytes);

  if (header.type != wire::MessageType::MasterMessage)
    throw wire::DecodeError("expected the master's reply, got something else");

  std::vector<std::byte> payload(header.payload_length);
  asio::read(channel_->socket(), asio::buffer(payload));
  auto message = decodePayload<MasterMessage>(payload);

  // The master sees loopback addresses for components on its own host.
  // Swap in the address we used to reach it so they are reachable for us too.
  for (auto& component : message.all_components)
    if (component.ip == "127.0.0.1") component.ip = master_ip_;

  return message;
}

asio::awaitable<void> MasterLink::run(LocalClient& local_client) {
  try {
    for (;;) {
      const auto frame = co_await channel_->read();
      if (frame.header.type != wire::MessageType::UpdateComponents)
        throw wire::DecodeError("the master sent an unexpected message type");

      auto update = decodePayload<UpdateComponents>(frame.bytes());
      if (update.component.ip == "127.0.0.1") update.component.ip = master_ip_;

      local_client.onComponentUpdate(update);
    }
  } catch (const std::exception& e) {
    if (closing_.load(std::memory_order_relaxed))
      SRNP_DEBUG("stopped listening to the master");
    else
      SRNP_ERROR("lost the connection to the master: {}", e.what());
  }
}

void MasterLink::close() {
  closing_.store(true, std::memory_order_relaxed);
  channel_->close();
}

/** SERVER **/

Server::Server(asio::io_context& io, std::string master_ip, std::string master_port,
               PairSpace& pair_space, LocalClient& local_client, int desired_owner_id)
    : io_(io),
      acceptor_strand_(asio::make_strand(io)),
      acceptor_(io, tcp::endpoint(tcp::v4(), 0)),
      pair_space_(pair_space),
      local_client_(local_client) {
  port_ = acceptor_.local_endpoint().port();

  master_link_ = std::make_unique<MasterLink>(io_, std::move(master_ip), std::move(master_port));
  const MasterMessage welcome = master_link_->registerWithMaster(port_, desired_owner_id);
  owner_id_.store(welcome.owner, std::memory_order_relaxed);
  SRNP_INFO("registered with the master as owner {}", welcome.owner);

  // Our client has no other way to learn its owner id or its peers. This
  // used to travel over the loopback connection between the two halves.
  local_client_.onWelcome(welcome);

  asio::co_spawn(acceptor_strand_, acceptLoop(), asio::detached);
  asio::co_spawn(master_link_->strand(), master_link_->run(local_client_), asio::detached);
  startWorkers();
}

void Server::startWorkers() {
  // Four threads so a user callback that blocks doesn't stall everything else.
  for (int i = 0; i < 4; ++i) workers_.emplace_back([this] { io_.run(); });
}

asio::awaitable<void> Server::acceptLoop() {
  for (;;) {
    auto [error, socket] =
        co_await acceptor_.async_accept(asio::as_tuple(asio::use_awaitable));
    if (error) {
      if (!stopped_.load(std::memory_order_relaxed))
        SRNP_ERROR("stopped accepting connections: {}", error.message());
      co_return;
    }

    auto session =
        std::make_shared<ServerSession>(std::move(socket), asio::make_strand(io_), *this);
    auto& strand = session->strand();
    asio::co_spawn(strand, session->run(), asio::detached);
  }
}

void Server::printPairSpace() {
  std::lock_guard lock(pair_space_.mutex);
  pair_space_.printPairSpace();
}

void Server::stop() {
  if (stopped_.exchange(true)) return;

  if (master_link_) master_link_->close();

  // Hand the acceptor's close to the strand that runs the accept loop.
  asio::post(acceptor_strand_, [this] {
    boost::system::error_code ignored;
    acceptor_.close(ignored);
  });

  io_.stop();
  // jthread joins on destruction, but clearing here makes shutdown ordering
  // obvious and lets stop() be called before the destructor runs.
  workers_.clear();
}

Server::~Server() { stop(); }

}  // namespace srnp

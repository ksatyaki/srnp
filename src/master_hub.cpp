/*
  master_hub.cpp

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
#include <srnp/master_hub.h>
#include <srnp/msgs/codec.h>
#include <srnp/srnp_print.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <charconv>
#include <cstdlib>
#include <string>
#include <utility>

namespace srnp {

/** MASTER HUB SESSION **/

MasterHubSession::MasterHubSession(tcp::socket socket, Strand strand, MasterHub& master)
    : channel_(std::make_shared<FrameChannel>(std::move(socket), std::move(strand))),
      master_(master) {}

std::string MasterHubSession::address() const {
  boost::system::error_code error;
  const auto endpoint = channel_->socket().remote_endpoint(error);
  return error ? std::string() : endpoint.address().to_string();
}

asio::awaitable<void> MasterHubSession::run() {
  auto self = shared_from_this();

  try {
    // The handshake is just the first frame of the normal read loop, so a
    // slow component can no longer stall the accept path.
    const auto hello = co_await channel_->read();
    if (hello.header.type != wire::MessageType::IndicatePresence)
      throw wire::DecodeError("a component must introduce itself first");

    const auto presence = decodePayload<IndicatePresence>(hello.bytes());
    port_ = presence.port;

    const auto welcome = master_.registerComponent(self, presence);
    owner_ = welcome.owner;

    send(wire::frameOf(wire::MessageType::MasterMessage, welcome));
    SRNP_INFO("component on port {} registered as owner {}", port_, owner_);

    UpdateComponents added;
    added.operation = UpdateComponents::Operation::Add;
    added.component = ComponentInfo{owner_, address(), port_};
    master_.sendToAll(added);

    // Nothing else is expected. The next read finishes when they disconnect.
    co_await channel_->read();
    throw wire::DecodeError("a component sent us an unexpected message");
  } catch (const std::exception& e) {
    SRNP_DEBUG("component {} is done: {}", owner_, e.what());
  }

  if (owner_ != kAnyOwner) {
    master_.removeComponent(owner_);

    UpdateComponents removed;
    removed.operation = UpdateComponents::Operation::Remove;
    removed.component.owner = owner_;
    master_.sendToAll(removed);

    SRNP_INFO("component {} disconnected", owner_);
  }
  channel_->close();
}

/** MASTER HUB **/

MasterHub::MasterHub(asio::io_context& io, unsigned short port)
    : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
  port_ = acceptor_.local_endpoint().port();
  asio::co_spawn(io_, acceptLoop(), asio::detached);
  SRNP_INFO("master listening on port {}", port_);
}

asio::awaitable<void> MasterHub::acceptLoop() {
  for (;;) {
    auto [error, socket] =
        co_await acceptor_.async_accept(asio::as_tuple(asio::use_awaitable));
    if (error) {
      SRNP_INFO("master stopped accepting connections: {}", error.message());
      co_return;
    }
    auto session =
        std::make_shared<MasterHubSession>(std::move(socket), asio::make_strand(io_), *this);
    auto& strand = session->strand();
    asio::co_spawn(strand, session->run(), asio::detached);
  }
}

int MasterHub::makeOwnerId(const std::string& port) {
  int port_number = 0;
  std::from_chars(port.data(), port.data() + port.size(), port_number);

  // Seeding from the port keeps ids stable for a component that restarts on
  // the same port, which makes logs easier to follow.
  generator_.seed(static_cast<std::mt19937::result_type>(port_number));
  std::uniform_int_distribution<int> distribution(1000, 10000);

  int candidate = distribution(generator_);
  while (sessions_.contains(candidate)) ++candidate;
  return candidate;
}

MasterMessage MasterHub::registerComponent(const MasterHubSessionPtr& session,
                                           const IndicatePresence& presence) {
  std::lock_guard lock(mutex_);

  MasterMessage welcome;
  if (presence.force_owner_id && !sessions_.contains(presence.owner_id)) {
    welcome.owner = presence.owner_id;
  } else {
    if (presence.force_owner_id)
      SRNP_WARN("owner {} is taken, assigning a different one", presence.owner_id);
    welcome.owner = makeOwnerId(presence.port);
  }

  // Everyone who registered before this component, so it can reach them.
  for (const auto& [owner, other] : sessions_)
    welcome.all_components.push_back(ComponentInfo{owner, other->address(), other->port()});

  sessions_.emplace(welcome.owner, session);
  return welcome;
}

void MasterHub::removeComponent(int owner) {
  std::lock_guard lock(mutex_);
  sessions_.erase(owner);
}

void MasterHub::sendToAll(const UpdateComponents& update) {
  const auto frame = wire::frameOf(wire::MessageType::UpdateComponents, update);

  std::vector<MasterHubSessionPtr> targets;
  {
    std::lock_guard lock(mutex_);
    for (const auto& [owner, session] : sessions_)
      // The component this update is about already knows.
      if (owner != update.component.owner) targets.push_back(session);
  }

  for (const auto& session : targets) session->send(frame);
}

}  // namespace srnp

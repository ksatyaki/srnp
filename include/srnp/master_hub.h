/*
  master_hub.h - The Master Hub

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

#ifndef MASTER_HUB_H_
#define MASTER_HUB_H_

#include <srnp/msgs/MasterMessages.h>
#include <srnp/session.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>

namespace srnp {

class MasterHub;

/// One component's connection to the master.
class MasterHubSession : public std::enable_shared_from_this<MasterHubSession> {
 public:
  MasterHubSession(tcp::socket socket, Strand strand, MasterHub& master);

  /// Handles registration, then waits for the component to disconnect.
  /// Must be spawned on strand().
  asio::awaitable<void> run();

  Strand& strand() { return channel_->strand(); }

  void send(std::vector<std::byte> frame) { channel_->send(std::move(frame)); }
  void close() { channel_->close(); }

  int owner() const { return owner_; }
  const std::string& port() const { return port_; }
  std::string address() const;

 private:
  FrameChannelPtr channel_;
  MasterHub& master_;
  int owner_ = kAnyOwner;
  std::string port_;
};

using MasterHubSessionPtr = std::shared_ptr<MasterHubSession>;

/**
 * Hands out owner ids and keeps every component's list of peers current.
 * It carries no pairs; components talk to each other directly.
 */
class MasterHub {
 public:
  MasterHub(asio::io_context& io, unsigned short port);

  /// The port actually bound, which matters when port 0 was requested.
  unsigned short port() const { return port_; }

  /// Registers a component and returns the reply to send it.
  MasterMessage registerComponent(const MasterHubSessionPtr& session,
                                  const IndicatePresence& presence);

  void removeComponent(int owner);

  void sendToAll(const UpdateComponents& update);

 private:
  asio::awaitable<void> acceptLoop();

  /// Derives an id from the component's port, then steps past any collision
  /// so two components can never share one.
  int makeOwnerId(const std::string& port);

  asio::io_context& io_;
  tcp::acceptor acceptor_;
  unsigned short port_ = 0;

  mutable std::mutex mutex_;
  std::map<int, MasterHubSessionPtr> sessions_;
  std::mt19937 generator_;
};

}  // namespace srnp

#endif /* MASTER_HUB_H_ */

/*
  server.h - Server sits behind the client and owns the pair space.

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

#ifndef SRNP_SERVER_H_
#define SRNP_SERVER_H_

#include <srnp/Pair.h>
#include <srnp/PairQueue.h>
#include <srnp/PairSpace.h>
#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MasterMessages.h>
#include <srnp/session.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace srnp {

class Server;

/**
 * One connection into our server. Either our own client on the loopback,
 * or another component pushing pairs and subscriptions at us.
 */
class ServerSession : public std::enable_shared_from_this<ServerSession> {
 public:
  ServerSession(tcp::socket socket, Strand strand, Server& server);

  /// Reads frames until the peer goes away. Owns itself for the duration.
  /// Must be spawned on strand(), which is what serialises socket access.
  asio::awaitable<void> run();

  Strand& strand() { return channel_->strand(); }

  /// Queues a pair update for our own client to fan out to subscribers.
  /// A subscriber other than kAnyOwner sends it to that one component only.
  void sendPairUpdate(const Pair& pair, int subscriber = kAnyOwner);

  void send(std::vector<std::byte> frame) { channel_->send(std::move(frame)); }
  void close() { channel_->close(); }

 private:
  void handle(const Frame& frame);
  void handleLocalPair();
  void handleIncomingPair(const Frame& frame);
  void handlePairUpdate(const Frame& frame);
  void handleSubscription(const Frame& frame);

  /// Applies the pair, then runs the callbacks outside the lock so a slow
  /// user callback can't block the rest of the pair space.
  Pair::ConstPtr applyAndNotify(const Pair& pair);

  FrameChannelPtr channel_;
  Server& server_;
};

using ServerSessionPtr = std::shared_ptr<ServerSession>;

/**
 * Keeps us registered with the master and tells us about other components.
 */
class MasterLink {
 public:
  MasterLink(asio::io_context& io, std::string master_ip, std::string master_port);

  Strand& strand() { return channel_->strand(); }

  /// Connects, registers, and blocks until the master replies with our id.
  MasterMessage registerWithMaster(unsigned short our_port, int desired_owner_id);

  /// Streams component add/remove notices to our own client until the master drops.
  asio::awaitable<void> run(ServerSessionPtr client_session);

  void close();

 private:
  std::string master_ip_;
  std::string master_port_;
  FrameChannelPtr channel_;
  /// Tells a shutdown apart from the master actually going away.
  std::atomic<bool> closing_{false};
};

class Server {
 public:
  Server(asio::io_context& io, std::string master_ip, std::string master_port,
         PairSpace& pair_space, PairQueue& pair_queue, int desired_owner_id = kAnyOwner);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  unsigned short getPort() const { return port_; }
  int owner() const { return owner_id_.load(std::memory_order_relaxed); }

  /// The session with our own client. Null until that client connects.
  ServerSessionPtr myClientSession() const;

  /// Called when a session identifies itself as our own client. Sends it
  /// the master's reply and starts streaming component updates to it.
  void attachClient(ServerSessionPtr session);

  PairSpace& pairSpace() { return pair_space_; }
  PairQueue& pairQueue() { return pair_queue_; }

  void printPairSpace();

  /// Stops the io_context and joins the worker threads. Safe to call twice.
  void stop();

 private:
  asio::awaitable<void> acceptLoop();
  void startWorkers();

  asio::io_context& io_;
  /// The acceptor is a socket too: closing it from another thread would
  /// race the accept in flight, so it lives on this strand.
  Strand acceptor_strand_;
  tcp::acceptor acceptor_;
  unsigned short port_ = 0;

  std::atomic<int> owner_id_{kAnyOwner};

  /// The master's reply, kept so it can be handed to our client as soon as
  /// it connects. Our client learns its owner id from this.
  MasterMessage welcome_;

  PairSpace& pair_space_;
  PairQueue& pair_queue_;

  mutable std::mutex client_session_mutex_;
  ServerSessionPtr my_client_session_;

  std::unique_ptr<MasterLink> master_link_;
  std::vector<std::jthread> workers_;
  std::atomic<bool> stopped_{false};
};

}  // namespace srnp

#endif /* SRNP_SERVER_H_ */

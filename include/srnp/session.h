/*
  session.h - Shared plumbing for reading and writing frames on a socket.

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

#ifndef SRNP_SESSION_H_
#define SRNP_SESSION_H_

#include <srnp/Pair.h>
#include <srnp/wire.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace srnp {

namespace asio = boost::asio;
using asio::ip::tcp;

using Strand = asio::strand<asio::io_context::executor_type>;

/// One decoded message, header and payload together.
struct Frame {
  wire::FrameHeader header;
  std::vector<std::byte> payload;

  std::span<const std::byte> bytes() const { return payload; }
};

/**
 * Owns a socket and gives it two operations: read whole frames, and send
 * whole frames. Sends are queued and written one at a time, so frames
 * never interleave and the buffer always outlives its write.
 *
 * A socket cannot be used from several threads at once, so everything that
 * touches it runs on one strand: reads, writes, and the close. Whoever
 * creates a channel must run its read loop on strand().
 */
class FrameChannel : public std::enable_shared_from_this<FrameChannel> {
 public:
  FrameChannel(tcp::socket socket, Strand strand);

  /// Small messages should go out immediately, so Nagle is turned off.
  static void configure(tcp::socket& socket);

  tcp::socket& socket() { return socket_; }
  Strand& strand() { return strand_; }

  /// Throws on a closed connection or a malformed frame.
  /// Must be called from a coroutine running on strand().
  asio::awaitable<Frame> read();

  /// Queues a frame. Returns once it is queued, not once it is written.
  /// Safe to call from any thread.
  ///
  /// `coalesce_key` names the pair a frame carries an update for, and marks
  /// the frame droppable. A subscriber that stops reading would otherwise
  /// grow our memory without limit; once the queue is full, a new update for
  /// a key already waiting replaces it in place. The subscriber then gets the
  /// current value instead of a stale backlog. Leave it unset for anything
  /// that is not last-value-wins — a subscription or a removal dropped in
  /// silence corrupts the peer's state.
  void send(std::vector<std::byte> frame,
            std::optional<PairKey> coalesce_key = std::nullopt);

  /// Safe to call from any thread. The socket itself is closed on the
  /// strand, so it never races a read or write in progress.
  void close();

  bool isClosed() const { return closed_.load(std::memory_order_acquire); }

  /// How many frames are waiting to be written. Exists so the backpressure
  /// test can watch the queue stay bounded; nothing else should need it.
  std::size_t outboxSize() const;

  /// The cap outboxSize() is held under.
  static std::size_t maxOutboxFrames();

 private:
  asio::awaitable<void> drainOutbox();

  tcp::socket socket_;
  Strand strand_;

  struct Outbound {
    std::vector<std::byte> bytes;
    /// Set only on pair updates, which are the only droppable frames.
    std::optional<PairKey> coalesce_key;
  };

  /// Called with outbox_mutex_ held. True if the frame found a home.
  bool makeRoomLocked(const std::optional<PairKey>& coalesce_key,
                      std::vector<std::byte>& frame);

  mutable std::mutex outbox_mutex_;
  /// A list, not a deque, so the index below keeps its iterators valid
  /// across a push at the back and an erase in the middle.
  std::list<Outbound> outbox_;
  /// Where each coalescable key currently sits, so replacing one is a lookup
  /// rather than a walk of the whole queue.
  std::map<PairKey, std::list<Outbound>::iterator> coalescable_;
  bool writing_ = false;
  std::atomic<bool> closed_{false};
};

using FrameChannelPtr = std::shared_ptr<FrameChannel>;

}  // namespace srnp

#endif /* SRNP_SESSION_H_ */

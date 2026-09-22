/*
  session.cpp - Implementation of FrameChannel.

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

#include <srnp/session.h>
#include <srnp/srnp_print.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <array>
#include <utility>

namespace srnp {

/// How many frames to hold for a peer that has stopped reading. At the cap,
/// pair updates coalesce by key and then drop oldest-first; a connection with
/// nothing droppable left is closed rather than allowed to grow.
constexpr std::size_t kMaxOutboxFrames = 4096;

FrameChannel::FrameChannel(tcp::socket socket, Strand strand)
    : socket_(std::move(socket)), strand_(std::move(strand)) {
  configure(socket_);
}

void FrameChannel::configure(tcp::socket& socket) {
  boost::system::error_code ignored;
  socket.set_option(tcp::no_delay(true), ignored);
}

asio::awaitable<Frame> FrameChannel::read() {
  std::array<std::byte, wire::kHeaderSize> header_bytes{};
  co_await asio::async_read(socket_, asio::buffer(header_bytes), asio::use_awaitable);

  Frame frame;
  frame.header = wire::decodeHeader(header_bytes);

  if (frame.header.payload_length > 0) {
    frame.payload.resize(frame.header.payload_length);
    co_await asio::async_read(socket_, asio::buffer(frame.payload), asio::use_awaitable);
  }
  co_return frame;
}

bool FrameChannel::makeRoomLocked(const std::optional<PairKey>& coalesce_key,
                                  std::vector<std::byte>& frame) {
  if (outbox_.size() < kMaxOutboxFrames) return false;

  // This key is already waiting: overwrite it rather than queue a second
  // copy. The subscriber has not seen the older value and never will.
  if (coalesce_key) {
    if (const auto it = coalescable_.find(*coalesce_key); it != coalescable_.end()) {
      it->second->bytes = std::move(frame);
      return true;
    }
  }

  // Otherwise make room by dropping the oldest frame that is safe to drop.
  for (auto it = outbox_.begin(); it != outbox_.end(); ++it) {
    if (!it->coalesce_key) continue;
    coalescable_.erase(*it->coalesce_key);
    outbox_.erase(it);
    return false;
  }

  // Nothing in the queue may be dropped, so the peer is far enough behind
  // that we cannot serve it without lying about what it has received.
  SRNP_WARN("a peer is {} frames behind with nothing droppable; closing the connection",
            outbox_.size());
  return false;
}

void FrameChannel::send(std::vector<std::byte> frame, std::optional<PairKey> coalesce_key) {
  if (isClosed()) {
    SRNP_DEBUG("dropping a frame for a closed connection");
    return;
  }

  bool overfull = false;
  {
    std::lock_guard lock(outbox_mutex_);
    if (makeRoomLocked(coalesce_key, frame)) return;  // Replaced in place.

    if (outbox_.size() >= kMaxOutboxFrames) {
      overfull = true;
    } else {
      outbox_.push_back(Outbound{std::move(frame), coalesce_key});
      if (coalesce_key) coalescable_.insert_or_assign(*coalesce_key, std::prev(outbox_.end()));
      if (writing_) return;  // The running drain will pick it up.
      writing_ = true;
    }
  }

  if (overfull) {
    close();
    return;
  }

  asio::co_spawn(strand_, [self = shared_from_this()] { return self->drainOutbox(); },
                 asio::detached);
}

asio::awaitable<void> FrameChannel::drainOutbox() {
  for (;;) {
    std::vector<std::byte> next;
    {
      std::lock_guard lock(outbox_mutex_);
      if (outbox_.empty() || isClosed()) {
        writing_ = false;
        co_return;
      }
      next = std::move(outbox_.front().bytes);
      if (outbox_.front().coalesce_key) coalescable_.erase(*outbox_.front().coalesce_key);
      outbox_.pop_front();
    }

    // One write per frame keeps header and payload together on the wire.
    auto [error, _] = co_await asio::async_write(
        socket_, asio::buffer(next),
        asio::as_tuple(asio::use_awaitable));

    if (error) {
      SRNP_DEBUG("write failed, dropping queued frames: {}", error.message());
      std::lock_guard lock(outbox_mutex_);
      outbox_.clear();
      coalescable_.clear();
      writing_ = false;
      co_return;
    }
  }
}

std::size_t FrameChannel::outboxSize() const {
  std::lock_guard lock(outbox_mutex_);
  return outbox_.size();
}

std::size_t FrameChannel::maxOutboxFrames() { return kMaxOutboxFrames; }

void FrameChannel::close() {
  if (closed_.exchange(true, std::memory_order_acq_rel)) return;

  {
    std::lock_guard lock(outbox_mutex_);
    outbox_.clear();
    coalescable_.clear();
  }

  // Closing the socket here would race a read or write already in flight on
  // another thread, so hand it to the strand that owns the socket.
  asio::post(strand_, [self = shared_from_this()] {
    boost::system::error_code ignored;
    self->socket_.shutdown(tcp::socket::shutdown_both, ignored);
    self->socket_.close(ignored);
  });
}

}  // namespace srnp

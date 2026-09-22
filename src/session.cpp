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

void FrameChannel::send(std::vector<std::byte> frame) {
  if (isClosed()) {
    SRNP_DEBUG("dropping a frame for a closed connection");
    return;
  }

  {
    std::lock_guard lock(outbox_mutex_);
    outbox_.push_back(std::move(frame));
    if (writing_) return;  // The running drain will pick it up.
    writing_ = true;
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
      next = std::move(outbox_.front());
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
      writing_ = false;
      co_return;
    }
  }
}

void FrameChannel::close() {
  if (closed_.exchange(true, std::memory_order_acq_rel)) return;

  {
    std::lock_guard lock(outbox_mutex_);
    outbox_.clear();
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

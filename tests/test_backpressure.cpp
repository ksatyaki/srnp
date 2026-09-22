// A subscriber that stops reading must not grow the publisher without bound,
// and must see the current value per key when it comes back rather than a
// stale backlog.

#include <srnp/session.h>
#include <srnp/wire.h>

#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

using boost::asio::ip::tcp;
namespace asio = boost::asio;

/// A connected pair of sockets whose buffers are deliberately tiny, so the
/// outbox backs up after a handful of frames instead of megabytes.
struct TinyLink {
  asio::io_context io;
  tcp::socket sender{io};
  tcp::socket receiver{io};

  TinyLink() {
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    sender.connect(acceptor.local_endpoint());
    acceptor.accept(receiver);

    const asio::socket_base::send_buffer_size send_size(2048);
    const asio::socket_base::receive_buffer_size receive_size(2048);
    sender.set_option(send_size);
    receiver.set_option(receive_size);
  }
};

srnp::PairKey keyOf(int owner, std::string key) { return srnp::PairKey{owner, std::move(key)}; }

std::vector<std::byte> payloadFrame(std::size_t size) {
  return srnp::wire::frame(srnp::wire::MessageType::PairUpdate,
                           std::vector<std::byte>(size, std::byte{'x'}));
}

}  // namespace

TEST(Backpressure, OneKeyFloodedStaysBounded) {
  TinyLink link;
  auto channel = std::make_shared<srnp::FrameChannel>(std::move(link.sender),
                                                      asio::make_strand(link.io));
  std::jthread worker([&link] { link.io.run(); });

  const std::size_t cap = srnp::FrameChannel::maxOutboxFrames();
  // Nobody reads link.receiver, so every frame past the socket buffer piles
  // up in the outbox. Far more than the cap, all for the same key.
  for (std::size_t i = 0; i < cap * 3; ++i)
    channel->send(payloadFrame(512), keyOf(1, "hot"));

  EXPECT_LE(channel->outboxSize(), cap)
      << "a peer that stopped reading grew the queue past its cap";
  EXPECT_FALSE(channel->isClosed()) << "a coalescable flood should not close the connection";

  channel->close();
  link.io.stop();
}

TEST(Backpressure, ManyKeysAlsoStayBounded) {
  TinyLink link;
  auto channel = std::make_shared<srnp::FrameChannel>(std::move(link.sender),
                                                      asio::make_strand(link.io));
  std::jthread worker([&link] { link.io.run(); });

  const std::size_t cap = srnp::FrameChannel::maxOutboxFrames();
  for (std::size_t i = 0; i < cap * 3; ++i)
    channel->send(payloadFrame(512), keyOf(1, "key" + std::to_string(i)));

  // Distinct keys cannot coalesce, so these drop oldest-first instead.
  EXPECT_LE(channel->outboxSize(), cap);

  channel->close();
  link.io.stop();
}

TEST(Backpressure, ControlFramesAreNeverDropped) {
  TinyLink link;
  auto channel = std::make_shared<srnp::FrameChannel>(std::move(link.sender),
                                                      asio::make_strand(link.io));
  std::jthread worker([&link] { link.io.run(); });

  const std::size_t cap = srnp::FrameChannel::maxOutboxFrames();
  // No coalesce key means not droppable. Once the queue is full of frames
  // that may not be dropped, the only honest move left is to close.
  for (std::size_t i = 0; i < cap + 16; ++i)
    channel->send(srnp::wire::frame(srnp::wire::MessageType::Subscription,
                                    std::vector<std::byte>(512, std::byte{'s'})));

  EXPECT_TRUE(channel->isClosed())
      << "a peer too far behind to serve without dropping control frames must be closed";

  link.io.stop();
}

TEST(Backpressure, TheSurvivingFrameIsTheNewestValue) {
  TinyLink link;
  auto channel = std::make_shared<srnp::FrameChannel>(std::move(link.sender),
                                                      asio::make_strand(link.io));
  std::jthread worker([&link] { link.io.run(); });

  // Every frame carries its sequence number as the payload, all under one
  // key. Once the queue is full these replace each other in place.
  const std::size_t total = srnp::FrameChannel::maxOutboxFrames() * 3;
  for (std::size_t i = 0; i < total; ++i) {
    const std::string body = std::to_string(i);
    std::vector<std::byte> bytes(body.size());
    std::memcpy(bytes.data(), body.data(), body.size());
    channel->send(srnp::wire::frame(srnp::wire::MessageType::PairUpdate, bytes),
                  keyOf(1, "hot"));
  }

  // Now drain. The reader should reach the newest value, never stop short
  // at a stale one, because the newest overwrote whatever was queued.
  std::string newest;
  boost::system::error_code error;
  for (;;) {
    std::array<std::byte, srnp::wire::kHeaderSize> header{};
    asio::read(link.receiver, asio::buffer(header), error);
    if (error) break;
    const auto decoded = srnp::wire::decodeHeader(header);
    std::vector<std::byte> payload(decoded.payload_length);
    if (decoded.payload_length != 0) {
      asio::read(link.receiver, asio::buffer(payload), error);
      if (error) break;
    }
    newest.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
    if (newest == std::to_string(total - 1)) break;
  }

  EXPECT_EQ(newest, std::to_string(total - 1))
      << "the subscriber was left holding a stale value instead of the current one";

  channel->close();
  link.io.stop();
}

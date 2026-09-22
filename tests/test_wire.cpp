#include <srnp/msgs/codec.h>
#include <srnp/wire.h>

#include <gtest/gtest.h>

using namespace srnp;

namespace {

/// Encodes a message and decodes it straight back.
template <class T>
T roundTrip(const T& original) {
  wire::Writer writer;
  encode(writer, original);
  return decodePayload<T>(std::move(writer).take());
}

}  // namespace

TEST(Wire, PairRoundTrip) {
  Pair original(1234, "some/key", "a value with spaces", Pair::Type::Bytes);
  original.setWriteTime(Clock::now());
  original.setExpiryTime(Clock::now() + std::chrono::seconds(30));

  const Pair decoded = roundTrip(original);

  EXPECT_EQ(decoded.getOwner(), original.getOwner());
  EXPECT_EQ(decoded.getKey(), original.getKey());
  EXPECT_EQ(decoded.getValue(), original.getValue());
  EXPECT_EQ(decoded.getType(), original.getType());
  EXPECT_EQ(decoded.getWriteTime(), original.getWriteTime());
  EXPECT_EQ(decoded.getExpiryTime(), original.getExpiryTime());
}

TEST(Wire, PairWithEmptyAndBinaryValues) {
  Pair empty(1, "", "", Pair::Type::Invalid);
  EXPECT_EQ(roundTrip(empty).getValue(), "");

  // Embedded nulls must survive: the length prefix is what makes that work.
  Pair binary(2, "blob", std::string("a\0b\0c", 5), Pair::Type::Bytes);
  EXPECT_EQ(roundTrip(binary).getValue(), std::string("a\0b\0c", 5));
}

TEST(Wire, SubscriptionRoundTrip) {
  Subscription original{"key", 7, 9, false};
  const auto decoded = roundTrip(original);

  EXPECT_EQ(decoded.key, original.key);
  EXPECT_EQ(decoded.owner_id, original.owner_id);
  EXPECT_EQ(decoded.subscriber, original.subscriber);
  EXPECT_EQ(decoded.registering, original.registering);
}

TEST(Wire, IndicatePresenceRoundTrip) {
  IndicatePresence original{"54321", true, 4242};
  const auto decoded = roundTrip(original);

  EXPECT_EQ(decoded.port, original.port);
  EXPECT_EQ(decoded.force_owner_id, original.force_owner_id);
  EXPECT_EQ(decoded.owner_id, original.owner_id);
}

TEST(Wire, UpdateComponentsRoundTrip) {
  UpdateComponents original;
  original.operation = UpdateComponents::Operation::Remove;
  original.component = ComponentInfo{11, "192.168.1.5", "5000"};

  const auto decoded = roundTrip(original);
  EXPECT_EQ(decoded.operation, original.operation);
  EXPECT_EQ(decoded.component.owner, original.component.owner);
  EXPECT_EQ(decoded.component.ip, original.component.ip);
  EXPECT_EQ(decoded.component.port, original.component.port);
}

TEST(Wire, MasterMessageRoundTrip) {
  MasterMessage original;
  original.owner = 55;
  original.all_components = {ComponentInfo{1, "127.0.0.1", "100"},
                             ComponentInfo{2, "10.0.0.2", "200"}};

  const auto decoded = roundTrip(original);
  ASSERT_EQ(decoded.all_components.size(), 2u);
  EXPECT_EQ(decoded.owner, 55);
  EXPECT_EQ(decoded.all_components[1].ip, "10.0.0.2");
}

TEST(Wire, MasterMessageWithNoComponents) {
  MasterMessage original;
  original.owner = 1;
  EXPECT_TRUE(roundTrip(original).all_components.empty());
}

TEST(Wire, HeaderRoundTrip) {
  wire::FrameHeader original;
  original.type = wire::MessageType::PairUpdateOne;
  original.payload_length = 4242;
  original.subscriber = 99;

  const auto bytes = wire::encodeHeader(original);
  ASSERT_EQ(bytes.size(), wire::kHeaderSize);

  const auto decoded = wire::decodeHeader(bytes);
  EXPECT_EQ(decoded.type, original.type);
  EXPECT_EQ(decoded.payload_length, original.payload_length);
  EXPECT_EQ(decoded.subscriber, original.subscriber);
}

TEST(Wire, FramePutsHeaderAndPayloadTogether) {
  const Pair pair(1, "key", "value");
  const auto bytes = wire::frameOf(wire::MessageType::Pair, pair);

  const auto header = wire::decodeHeader(std::span(bytes).first(wire::kHeaderSize));
  EXPECT_EQ(header.type, wire::MessageType::Pair);
  EXPECT_EQ(bytes.size(), wire::kHeaderSize + header.payload_length);

  const auto decoded =
      decodePayload<Pair>(std::span(bytes).subspan(wire::kHeaderSize));
  EXPECT_EQ(decoded.getKey(), "key");
}

TEST(Wire, HeaderOfTheWrongSizeIsRejected) {
  std::vector<std::byte> too_short(wire::kHeaderSize - 1);
  EXPECT_THROW(wire::decodeHeader(too_short), wire::DecodeError);
}

TEST(Wire, WrongMagicIsRejected) {
  auto bytes = wire::encodeHeader({wire::MessageType::Pair, 0, -1});
  bytes[0] = std::byte{0xFF};
  EXPECT_THROW(wire::decodeHeader(bytes), wire::DecodeError);
}

TEST(Wire, UnknownVersionIsRejected) {
  auto bytes = wire::encodeHeader({wire::MessageType::Pair, 0, -1});
  bytes[4] = std::byte{99};
  EXPECT_THROW(wire::decodeHeader(bytes), wire::DecodeError);
}

TEST(Wire, UnknownMessageTypeIsRejected) {
  auto bytes = wire::encodeHeader({wire::MessageType::Pair, 0, -1});
  bytes[5] = std::byte{200};
  EXPECT_THROW(wire::decodeHeader(bytes), wire::DecodeError);

  bytes[5] = std::byte{0};  // Invalid is never legal on the wire either.
  EXPECT_THROW(wire::decodeHeader(bytes), wire::DecodeError);
}

TEST(Wire, OversizedPayloadIsRejected) {
  // This is the check that stops a hostile peer asking for a huge allocation.
  auto bytes = wire::encodeHeader({wire::MessageType::Pair, wire::kMaxPayload + 1, -1});
  EXPECT_THROW(wire::decodeHeader(bytes), wire::DecodeError);
}

TEST(Wire, TruncatedPayloadIsRejected) {
  wire::Writer writer;
  encode(writer, Pair(1, "key", "value"));
  auto bytes = std::move(writer).take();
  bytes.resize(bytes.size() / 2);

  EXPECT_THROW(decodePayload<Pair>(bytes), wire::DecodeError);
}

TEST(Wire, TrailingBytesAreRejected) {
  wire::Writer writer;
  encode(writer, Subscription{"key", 1, 2, true});
  auto bytes = std::move(writer).take();
  bytes.push_back(std::byte{0});

  EXPECT_THROW(decodePayload<Subscription>(bytes), wire::DecodeError);
}

TEST(Wire, AStringLengthPastTheBufferIsRejected) {
  // Claims a 4 GB string in a handful of bytes.
  wire::Writer writer;
  writer.integer<std::uint32_t>(0xFFFFFFFF);
  EXPECT_THROW(decodePayload<Subscription>(std::move(writer).take()), wire::DecodeError);
}

TEST(Wire, AnOutOfRangeEnumIsRejected) {
  wire::Writer writer;
  writer.integer<std::int32_t>(1);
  writer.string("key");
  writer.string("value");
  writer.integer<std::uint8_t>(200);  // No such Pair::Type.
  writer.integer<std::int64_t>(0);
  writer.integer<std::int64_t>(0);

  EXPECT_THROW(decodePayload<Pair>(std::move(writer).take()), wire::DecodeError);
}

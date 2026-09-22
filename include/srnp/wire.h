/*
  wire.h - The byte format used on every socket.

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

#ifndef SRNP_WIRE_H_
#define SRNP_WIRE_H_

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace srnp::wire {

/**
 * Every message is a 16 byte header followed by an optional payload:
 *
 *   u32 magic | u8 version | u8 type | u16 flags | u32 payload_length | u32 subscriber
 *
 * All integers are little-endian. 'subscriber' is only meaningful for
 * PairUpdateOne; the other message types leave it at -1.
 */
inline constexpr std::uint32_t kMagic = 0x504E5253;  // "SRNP"
inline constexpr std::uint8_t kVersion = 1;
inline constexpr std::size_t kHeaderSize = 16;

/// Anything bigger than this is treated as a corrupt or hostile peer.
inline constexpr std::uint32_t kMaxPayload = 16u * 1024 * 1024;

enum class MessageType : std::uint8_t {
  Invalid = 0,
  /// Register or cancel a subscription on someone else's pair.
  Subscription,
  /// A pair pushed to another component's server.
  Pair,
  /// Apply the pair waiting in our local queue. Never leaves the process.
  PairNoCopy,
  /// Tell our client to forward a pair to everyone subscribed to it.
  PairUpdate,
  /// Same, but to a single subscriber named in the header.
  PairUpdateOne,
  /// A component telling the master it exists and where to reach it.
  IndicatePresence,
  /// The master's reply with our owner id and the components it knows.
  MasterMessage,
  /// The master telling us a component appeared or disappeared.
  UpdateComponents,
  /// Sent by our own client so the server can tell it apart from the
  /// clients of other components, which also connect to us.
  AttachClient,
};

struct FrameHeader {
  MessageType type = MessageType::Invalid;
  std::uint32_t payload_length = 0;
  std::int32_t subscriber = -1;
};

/// Thrown when bytes off the socket don't make sense. Always fatal for that connection.
struct DecodeError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

template <class T>
concept WireInteger = std::integral<T> && (!std::same_as<T, bool>);

/// Byte-swaps if the host is big-endian. std::byteswap is C++23, so this stands in.
template <WireInteger T>
constexpr T toLittleEndian(T value) {
  if constexpr (std::endian::native == std::endian::little) return value;

  auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  std::ranges::reverse(bytes);
  return std::bit_cast<T>(bytes);
}

/**
 * Appends values to a byte buffer. Strings are length-prefixed, so a
 * decoder never has to guess where one ends.
 */
class Writer {
 public:
  template <WireInteger T>
  void integer(T value) {
    const auto little = toLittleEndian(value);
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(little);
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
  }

  void boolean(bool value) { integer<std::uint8_t>(value ? 1 : 0); }

  void string(std::string_view value) {
    if (value.size() > kMaxPayload) throw DecodeError("string too long to encode");
    integer<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
    const auto* first = reinterpret_cast<const std::byte*>(value.data());
    buffer_.insert(buffer_.end(), first, first + value.size());
  }

  std::vector<std::byte> take() && { return std::move(buffer_); }
  std::size_t size() const { return buffer_.size(); }

 private:
  std::vector<std::byte> buffer_;
};

/**
 * Reads values back out. Every read is bounds-checked, so a truncated or
 * malformed message throws instead of running off the end of the buffer.
 */
class Reader {
 public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

  template <WireInteger T>
  T integer() {
    require(sizeof(T));
    std::array<std::byte, sizeof(T)> raw{};
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), sizeof(T), raw.begin());
    offset_ += sizeof(T);
    return toLittleEndian(std::bit_cast<T>(raw));
  }

  bool boolean() { return integer<std::uint8_t>() != 0; }

  std::string string() {
    const auto length = integer<std::uint32_t>();
    require(length);
    const auto* first = reinterpret_cast<const char*>(bytes_.data() + offset_);
    offset_ += length;
    return std::string(first, length);
  }

  bool exhausted() const { return offset_ == bytes_.size(); }

 private:
  void require(std::size_t count) const {
    if (offset_ + count > bytes_.size()) throw DecodeError("message ended early");
  }

  std::span<const std::byte> bytes_;
  std::size_t offset_ = 0;
};

/// Writes a header into exactly kHeaderSize bytes.
std::vector<std::byte> encodeHeader(const FrameHeader& header);

/// Reads a header, rejecting a wrong magic, an unknown version or an absurd length.
FrameHeader decodeHeader(std::span<const std::byte> bytes);

/**
 * Builds a complete frame in one buffer so it can go out in a single write.
 * Splitting header and payload across writes is what invites Nagle delays.
 */
std::vector<std::byte> frame(MessageType type, std::span<const std::byte> payload,
                             std::int32_t subscriber = -1);

inline std::vector<std::byte> frame(MessageType type) { return frame(type, {}); }

/// Same, for any message with an encode() overload.
template <class T>
std::vector<std::byte> frameOf(MessageType type, const T& message, std::int32_t subscriber = -1) {
  Writer writer;
  encode(writer, message);
  return frame(type, std::move(writer).take(), subscriber);
}

}  // namespace srnp::wire

#endif /* SRNP_WIRE_H_ */

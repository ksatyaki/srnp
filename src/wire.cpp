/*
  wire.cpp - Frame header encoding and validation.

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

#include <srnp/wire.h>

#include <format>

namespace srnp::wire {

std::vector<std::byte> encodeHeader(const FrameHeader& header) {
  Writer writer;
  writer.integer(kMagic);
  writer.integer(kVersion);
  writer.integer(static_cast<std::uint8_t>(header.type));
  writer.integer<std::uint16_t>(0);  // flags, reserved
  writer.integer(header.payload_length);
  writer.integer(header.subscriber);
  return std::move(writer).take();
}

FrameHeader decodeHeader(std::span<const std::byte> bytes) {
  if (bytes.size() != kHeaderSize) throw DecodeError("header is the wrong size");

  Reader reader(bytes);
  if (reader.integer<std::uint32_t>() != kMagic)
    throw DecodeError("not an SRNP frame");
  if (const auto version = reader.integer<std::uint8_t>(); version != kVersion)
    throw DecodeError(std::format("unsupported protocol version {}", version));

  FrameHeader header;
  const auto type = reader.integer<std::uint8_t>();
  if (type == 0 || type > static_cast<std::uint8_t>(MessageType::AttachClient))
    throw DecodeError(std::format("unknown message type {}", type));
  header.type = static_cast<MessageType>(type);

  reader.integer<std::uint16_t>();  // flags, ignored for now
  header.payload_length = reader.integer<std::uint32_t>();
  if (header.payload_length > kMaxPayload)
    throw DecodeError(std::format("payload of {} bytes is over the limit", header.payload_length));

  header.subscriber = reader.integer<std::int32_t>();
  return header;
}

std::vector<std::byte> frame(MessageType type, std::span<const std::byte> payload,
                             std::int32_t subscriber) {
  if (payload.size() > kMaxPayload) throw DecodeError("payload is over the limit");

  FrameHeader header;
  header.type = type;
  header.payload_length = static_cast<std::uint32_t>(payload.size());
  header.subscriber = subscriber;

  auto bytes = encodeHeader(header);
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

}  // namespace srnp::wire

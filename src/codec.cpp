/*
  codec.cpp - Implementation of the wire encoding.

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

#include <srnp/msgs/codec.h>

namespace srnp {
namespace {

/// Times go over the wire as nanoseconds since the epoch, which is stable everywhere.
std::int64_t toNanos(TimePoint time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

TimePoint fromNanos(std::int64_t nanos) {
  return TimePoint(std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(nanos)));
}

/// Rejects an enum value we don't know rather than casting blindly into the type.
template <class Enum>
Enum readEnum(wire::Reader& reader, Enum highest) {
  const auto raw = reader.integer<std::underlying_type_t<Enum>>();
  if (raw > static_cast<std::underlying_type_t<Enum>>(highest))
    throw wire::DecodeError("enum value out of range");
  return static_cast<Enum>(raw);
}

}  // namespace

void encode(wire::Writer& writer, const Pair& pair) {
  writer.integer<std::int32_t>(pair.getOwner());
  writer.string(pair.getKey());
  writer.string(pair.getValue());
  writer.integer(static_cast<std::uint8_t>(pair.getType()));
  writer.integer<std::int64_t>(toNanos(pair.getWriteTime()));
  writer.integer<std::int64_t>(toNanos(pair.getExpiryTime()));
}

void decode(wire::Reader& reader, Pair& pair) {
  const auto owner = reader.integer<std::int32_t>();
  auto key = reader.string();
  auto value = reader.string();
  const auto type = readEnum(reader, Pair::Type::Meta);

  pair.setOwner(owner);
  pair.setPair(std::move(key), std::move(value));
  pair.setType(type);
  pair.setWriteTime(fromNanos(reader.integer<std::int64_t>()));
  pair.setExpiryTime(fromNanos(reader.integer<std::int64_t>()));
}

void encode(wire::Writer& writer, const Subscription& message) {
  writer.string(message.key);
  writer.integer<std::int32_t>(message.owner_id);
  writer.integer<std::int32_t>(message.subscriber);
  writer.boolean(message.registering);
}

void decode(wire::Reader& reader, Subscription& message) {
  message.key = reader.string();
  message.owner_id = reader.integer<std::int32_t>();
  message.subscriber = reader.integer<std::int32_t>();
  message.registering = reader.boolean();
}

void encode(wire::Writer& writer, const IndicatePresence& message) {
  writer.string(message.port);
  writer.boolean(message.force_owner_id);
  writer.integer<std::int32_t>(message.owner_id);
}

void decode(wire::Reader& reader, IndicatePresence& message) {
  message.port = reader.string();
  message.force_owner_id = reader.boolean();
  message.owner_id = reader.integer<std::int32_t>();
}

void encode(wire::Writer& writer, const ComponentInfo& message) {
  writer.integer<std::int32_t>(message.owner);
  writer.string(message.ip);
  writer.string(message.port);
}

void decode(wire::Reader& reader, ComponentInfo& message) {
  message.owner = reader.integer<std::int32_t>();
  message.ip = reader.string();
  message.port = reader.string();
}

void encode(wire::Writer& writer, const UpdateComponents& message) {
  writer.integer(static_cast<std::uint8_t>(message.operation));
  encode(writer, message.component);
}

void decode(wire::Reader& reader, UpdateComponents& message) {
  message.operation = readEnum(reader, UpdateComponents::Operation::Remove);
  decode(reader, message.component);
}

void encode(wire::Writer& writer, const MasterMessage& message) {
  writer.integer<std::int32_t>(message.owner);
  writer.integer<std::uint32_t>(static_cast<std::uint32_t>(message.all_components.size()));
  for (const auto& component : message.all_components) encode(writer, component);
}

void decode(wire::Reader& reader, MasterMessage& message) {
  message.owner = reader.integer<std::int32_t>();
  const auto count = reader.integer<std::uint32_t>();

  // Each component needs at least a few bytes, so a count far past what the
  // buffer could hold is corrupt. Checking stops a huge bogus allocation.
  if (count > wire::kMaxPayload / sizeof(ComponentInfo))
    throw wire::DecodeError("implausible component count");

  message.all_components.resize(count);
  for (auto& component : message.all_components) decode(reader, component);
}

}  // namespace srnp

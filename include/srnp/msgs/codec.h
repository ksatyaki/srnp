/*
  codec.h - Turns messages into bytes and back.

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

#ifndef SRNP_CODEC_H_
#define SRNP_CODEC_H_

#include <srnp/Pair.h>
#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MasterMessages.h>
#include <srnp/wire.h>

namespace srnp {

/**
 * Subscribers and callbacks are deliberately left out of the encoded Pair.
 * Both are local bookkeeping and mean nothing to the component receiving it.
 */
void encode(wire::Writer& writer, const Pair& pair);
void decode(wire::Reader& reader, Pair& pair);

void encode(wire::Writer& writer, const Subscription& message);
void decode(wire::Reader& reader, Subscription& message);

void encode(wire::Writer& writer, const IndicatePresence& message);
void decode(wire::Reader& reader, IndicatePresence& message);

void encode(wire::Writer& writer, const ComponentInfo& message);
void decode(wire::Reader& reader, ComponentInfo& message);

void encode(wire::Writer& writer, const UpdateComponents& message);
void decode(wire::Reader& reader, UpdateComponents& message);

void encode(wire::Writer& writer, const MasterMessage& message);
void decode(wire::Reader& reader, MasterMessage& message);

/// Decodes a whole payload, and rejects it if there are bytes left over.
template <class T>
T decodePayload(std::span<const std::byte> payload) {
  wire::Reader reader(payload);
  T message;
  decode(reader, message);
  if (!reader.exhausted()) throw wire::DecodeError("message had unexpected trailing bytes");
  return message;
}

}  // namespace srnp

#endif /* SRNP_CODEC_H_ */

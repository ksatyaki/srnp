/*
  MessageHeader.h - A Header to use with all messages.
  
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

#ifndef MESSAGE_HEADER_H_
#define MESSAGE_HEADER_H_

#include <boost/serialization/serialization.hpp>
#include <boost/shared_ptr.hpp>

namespace srnp
{

struct MessageHeader
{
	enum MessageType
	{
		// We reject incoming messages with this type.
		INVALID = 0,
		// This is the register/cancel subscriber call.
		SUBSCRIPTION,
		// Register/cancel callback. (Local only).
		CALLBACK,
		// To delete a pair in our pair-space.
		PAIR_DELETE,
		// To pop a pair from the queue and add it to our pair space. (Local only).
		PAIR_NOCOPY,
		// To update a pair.
		PAIR_UPDATE,
		// To update a pair only to one guy. 
		PAIR_UPDATE_2
	};

	/**
	 * C++11 supports scoped enums.
	 * But, we need to keep this in the MessageHeader scope.
	 */
	enum MasterMessageType
	{
		// MasterMessage is sent.
		MM = 100,
		// UpdateComponents is sent.
		UC
	};

	typedef boost::shared_ptr<MessageHeader> Ptr;

	/**
	 * The entire length of the Message excluding the header.
	 * The length of the header is fixed.
	 */
	size_t length;

	/**
	 * The type of the message.
	 * Must be one of REQUEST, RESPONSE or PAIR.
	 */
	uint8_t type;

	/**
	 * Only used in the case of PAIR_UPDATE_2
	 */
	int subscriber__;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & length;
		o_archive & type;
		o_archive & subscriber__;
	}

	MessageHeader(): length(0), type(INVALID) { }

	MessageHeader (const size_t& Length, const MessageType& Type): length (Length), type(Type) { }

};

}

#endif /* MESSAGE_HEADER_H_ */

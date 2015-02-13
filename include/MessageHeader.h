/*
 * MessageHeader.h
 *
 *  Created on: Jan 13, 2015
 *      Author: ace
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
		// This is the register subscriber call.
		REGISTER_SUBSCRIBER,
		// The cancel subscriber call.
		CANCEL_SUBSCRIBER,
		// Register Callback. (Local only).
		REGISTER_CALLBACK,
		// Cancel callback.
		CANCEL_CALLBACK,
		// To delete a pair in our pair-space.
		PAIR_DELETE,
		// To add a pair to our pair space.
		PAIR,
		// To pop a pair from the queue and add it to our pair space. (Local only).
		PAIR_NOCOPY,
		// To update a pair on the subscribed pair-space.
		PAIR_UPDATE
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

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & length;
		o_archive & type;
	}

	MessageHeader(): length(0), type(INVALID) { }

	MessageHeader (const size_t& Length, const MessageType& Type): length (Length), type(Type) { }

};

}

#endif /* MESSAGE_HEADER_H_ */

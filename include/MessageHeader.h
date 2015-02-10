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

enum MessageType
{
	INVALID = 0,
	REQUEST,
	RESPONSE,
	PAIR,
	PAIR_NOCOPY
};

struct MessageHeader
{
	typedef boost::shared_ptr<MessageHeader> Ptr;

	/**
	 * The entire length of the Message excluding the header.
	 * The length of the header is fixed.
	 */
	size_t length_;

	/**
	 * The type of the message.
	 * Must be one of REQUEST, RESPONSE or PAIR.
	 */
	uint8_t type_;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & length_;
		o_archive & type_;
	}

	MessageHeader(): length_(0), type_(INVALID) { }

	MessageHeader (const size_t& length, const MessageType& type): length_ (length), type_(type) { }

};

}

#endif /* MESSAGE_HEADER_H_ */

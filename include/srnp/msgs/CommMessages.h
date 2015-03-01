/*
 * CommMessages.h
 *
 *  Created on: Feb 11, 2015
 *      Author: ace
 */

#ifndef COMMMESSAGES_H_
#define COMMMESSAGES_H_

#include <string>
#include <boost/function.hpp>

namespace srnp
{

struct SubscriptionORCallback
{
	/**
	 * Key of the Pair on which to register the subscription / callback.
	 */
	std::string key;

	/**
	 * The owner_id for subscription / callback.
	 */
	int owner_id;

	/**
	 * The owner_id of the subscriber.
	 */
	int subscriber;

	/**
	 * To know if we are registering or cancelling.
	 */
	bool registering;

	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_id;
		o_archive & key;
		o_archive & subscriber;
		o_archive & registering;
	}

};

}




#endif /* INCLUDE_COMMMESSAGES_H_ */

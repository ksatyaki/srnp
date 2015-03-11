/*
  CommMessages.h - Defines common messages sent between client and
  server.
  
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

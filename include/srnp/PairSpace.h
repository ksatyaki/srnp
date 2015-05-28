/*
  PairSpace.h - The space of all pairs.
  
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
#ifndef PAIRSPACE_H_
#define PAIRSPACE_H_

#include <string>
#include <map>

#include <srnp/Pair.h>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <srnp/srnp_print.h>

namespace srnp
{

class PairSpace
{
	std::vector <Pair> pairs_;

	/**
	 * Every subscriber in this list is added to every new pair.
	 */
	std::vector <int> u_subscribers_;

	std::map <CallbackHandle, std::string> cbid_to_key_;

	CallbackHandle cbid_new_;

public:

	Pair::CallbackFunction u_callback_;

	std::string getKeyFromCBID(double cbid);
	
	boost::mutex mutex;
	
	PairSpace();

	/**
	 * Remove a pair from the space using its key.
	 */
	void removePair(const std::string& key);

	/**
	 * Remove a pair from the space using its iterator.
	 */
	inline void removePair(const std::vector <Pair>::iterator& iter)
	{
		if(iter != pairs_.end())
			pairs_.erase(iter);
	}

	inline const std::vector <Pair>& getAllPairs() { return pairs_; }
	
	/**
	 * Get the pair iterator with the key. Used only in local pair space.
	 */
	std::vector <Pair>::iterator getPairIteratorWithKey(const std::string& key);
	
	/**
	 * Add a pair or update a pair in the pair-space.
	 */
	void addPair(const Pair& pair);

	/**
	 * Add subscription. If there is no such tuple, a new tuple is added and a subscription is added on that.
	 */
	void addSubscription(const std::string& key, const int& subscriber);

	/**
	 * Add Subscription to all.
	 */
	void addSubscriptionToAll(const int& subscriber);

	/**
	 * Remove Subscription to all.
	 */
	void removeSubscriptionToAll(const int& subscriber);

	/**
	 * Remove subscription.
	 */
	void removeSubscription(const std::string& key, const int& subscriber);

	/**
	 * Remove callback.
	 */
	void removeCallback(const CallbackHandle& cbid);

	/**
	 * Add a callback.
	 */
	CallbackHandle addCallback(const std::string& key, Pair::CallbackFunction callback_fn);

	void addCallbackToAll(Pair::CallbackFunction callback_fn);

	/**
	 * Print the entire pair-space.
	 */
	void printPairSpace();

	//inline void mutexLock() { mutex_.lock(); }

	//inline void mutexUnlock() { mutex_.unlock(); }
};

} /* namespace srnp */

#endif /* PAIRSPACE_H_ */

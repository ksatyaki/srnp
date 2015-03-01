/*
 * PairSpace.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef PAIRSPACE_H_
#define PAIRSPACE_H_

#include <srnp/Pair.h>
#include <boost/shared_ptr.hpp>

#include <srnp/srnp_print.h>

namespace srnp
{

class PairSpace
{
	std::vector <Pair> pairs_;

public:
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
	 * Remove subscription.
	 */
	void removeSubscription(const std::string& key, const int& subscriber);

	/**
	 * Remove callback.
	 */
	void removeCallback(const std::string& key);

	/**
	 * Add a callback.
	 */
	void addCallback(const std::string& key, Pair::CallbackFunction callback_fn);

	/**
	 * Print the entire pair-space.
	 */
	void printPairSpace();
};

} /* namespace srnp */

#endif /* PAIRSPACE_H_ */

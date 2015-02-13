/*
 * PairSpace.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef PAIRSPACE_H_
#define PAIRSPACE_H_

#include <Pair.h>
#include <boost/shared_ptr.hpp>

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
	 * Get the pair as a boost::shared_ptr with the key.
	 */
	std::vector <Pair>::iterator getPairIteratorWithKey(const std::string& key);

	/**
	 * Add a pair or update a pair in the pair-space.
	 */
	void addPair(const Pair& pair);

	/**
	 * Print the entire pair-space.
	 */
	void printPairSpace();
};

} /* namespace srnp */

#endif /* PAIRSPACE_H_ */

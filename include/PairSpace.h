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
	PairSpace() { };

	/**
	 * Remove a pair from the space.
	 */
	inline void removePair(const std::vector <Pair>::iterator& iter)
	{
		if(iter != pairs_.end())
			pairs_.erase(iter);
	}

	/**
	 * Get the pair as a boost::shared_ptr with the key.
	 */
	std::vector <Pair>::iterator getPairIteratorWithKey(const std::string& key)
	{
		typedef std::vector <Pair> PairVector;
		for(PairVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			if(iter->getKey().compare(key) == 0)
			{
				return iter;
			}
		}

		return pairs_.end();
	}

	/**
	 * Add a pair or update a pair in the pair-space.
	 */
	void addPair(const Pair& pair)
	{
		std::vector<Pair>::iterator iter = getPairIteratorWithKey (pair.getKey());
		if(iter == pairs_.end())
		{
			//printf("Brand new pair added.\n");
			pairs_.push_back(pair);
		}
		else
		{
			//printf("Something with that key is there already. Updated.\n");
			pairs_.erase(iter);
			// TODO Update the value and the stamp.
			pairs_.push_back(pair);
		}
	}

	/**
	 * Print the entire pair-space.
	 */
	void printPairSpace()
	{
		std::cout<<"*********************";
		std::cout <<"\nALL PAIRS\n";
		std::cout<<"*********************\n";
		typedef std::vector <Pair> PairVector;
		for(PairVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			//boost::shared_ptr <Pair <PairValueType> > this_pair = boost::static_pointer_cast <PairValueType>(*iter);

			std::cout<<(*iter);
		}
		std::cout<<"*********************"<<std::endl;
	}
};

} /* namespace srnp */

#endif /* PAIRSPACE_H_ */

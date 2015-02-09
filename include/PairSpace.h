/*
 * PairSpace.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef PAIRSPACE_H_
#define PAIRSPACE_H_

#include <PairBase.h>
#include <Pair.h>
#include <boost/shared_ptr.hpp>

namespace srnp
{



class PairSpace
{
	std::vector <PairBasePtr> pairs_;

public:
	PairSpace() { };

	/**
	 * Remove a pair from the space.
	 */
	inline void removePair(const std::vector <PairBasePtr>::iterator& iter)
	{
		if(iter != pairs_.end())
			pairs_.erase(iter);
	}

	/**
	 * Get the pair as a boost::shared_ptr with the key.
	 */
	template <typename PairValueType>
	std::vector <PairBasePtr>::iterator getPairIteratorWithKey(const std::string& key)
	{
		typedef std::vector <boost::shared_ptr <PairBase> > PairSharedPtrVector;
		for(PairSharedPtrVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			boost::shared_ptr <Pair <PairValueType> > this_pair = boost::static_pointer_cast < Pair <PairValueType> >(*iter);

			if(this_pair->getKey().compare(key) == 0)
			{
				return iter;
			}
		}

		return pairs_.end();
	}

	/**
	 * Add a pair or update a pair in the pair-space.
	 */
	template <typename PairValueType>
	void addPair(const boost::shared_ptr <PairBase>& pair)
	{
		boost::shared_ptr <Pair <PairValueType> > this_pair = boost::static_pointer_cast < Pair <PairValueType> >(pair);
		std::vector<PairBasePtr>::iterator iter = getPairIteratorWithKey <PairValueType> (this_pair->getKey());
		if(iter == pairs_.end())
		{
			//printf("Brand new pair added.\n");
			pairs_.push_back(pair);
		}
		else
		{
			//printf("Something with that key is there already. Updated.\n");
			pairs_.erase(iter);
			pairs_.push_back(pair);
		}
	}

	/**
	 * Print the entire pair-space.
	 */
	void printPairSpace()
	{
		typedef std::vector <boost::shared_ptr <PairBase> > PairSharedPtrVector;
		for(PairSharedPtrVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			//boost::shared_ptr <Pair <PairValueType> > this_pair = boost::static_pointer_cast <PairValueType>(*iter);

			(*iter)->printOnScreen();
			std::cout<<std::endl<<"*********************"<<std::endl;
		}

	}
};

} /* namespace srnp */

#endif /* PAIRSPACE_H_ */

/*
 * PairBase.h
 *
 *  Created on: Feb 8, 2015
 *      Author: ace
 */

#ifndef PAIR_BASE_H_
#define PAIR_BASE_H_

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <stdio.h>


namespace srnp
{

class PairBase
{
	typedef boost::shared_ptr <PairBase> Ptr;

protected:
	/**
	 * The owner id for this pair.
	 */
	int owner_;

	/**
	 * A list of owner ids subscribed to this pair.
	 */
	std::vector <int> subscribers_;

public:

	inline int getOwner() const { return owner_; }

	PairBase () : owner_(0)
	{

	}

	/**
	 * Constructor with pair's values.
	 */
	PairBase (const int& owner) :
		owner_(owner)
	{
	}

	/**
	 * Provide support for cout.
	 */
	virtual void printOnScreen() = 0;
};

typedef boost::shared_ptr <PairBase> PairBasePtr;

}




#endif /* PAIRBASE_H_ */

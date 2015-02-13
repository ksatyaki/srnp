/*
 * PairSpace.cpp
 *
 *  Created on: Feb 12, 2015
 *      Author: ace
 */
#include <PairSpace.h>

namespace srnp
{

PairSpace::PairSpace ()
{

}

std::vector <Pair>::iterator PairSpace::getPairIteratorWithKey(const std::string& key)
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

void PairSpace::addPair(const Pair& pair)
{
	std::vector<Pair>::iterator iter = getPairIteratorWithKey (pair.getKey());
	if(iter == pairs_.end())
	{
		//printf("Brand new pair added.\n");
		pairs_.push_back(pair);
		pairs_.back().setWriteTime(boost::posix_time::microsec_clock::universal_time());
	}
	else
	{
		std::string value = pair.getValue();
		// Set the value.
		iter->setValue(value);
		// Update time stamp.
		iter->setWriteTime(boost::posix_time::microsec_clock::universal_time());
	}
}

void PairSpace::removePair(const std::string& key)
{
	removePair(getPairIteratorWithKey(key));
}

void PairSpace::printPairSpace()
{
	std::cout<<"*********************";
	std::cout <<"\nALL PAIRS\n";
	std::cout<<"*********************\n";
	typedef std::vector <Pair> PairVector;
	for(PairVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
	{
		std::cout<<(*iter);
	}
	std::cout<<"*********************"<<std::endl;
}

}





/*
 * PairSpace.cpp
 *
 *  Created on: Feb 12, 2015
 *      Author: ace
 */
#include <srnp/PairSpace.h>

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

void PairSpace::addCallback(const std::string& key, Pair::CallbackFunction callback_fn)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithKey(key);
	if(it == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "Adding callback (future).";
		Pair new_one (-1, key, "");
		new_one.callback_ = callback_fn;
		addPair(new_one);
	}
	else
	{
		//SRNP_PRINT_DEBUG << "Adding a callback on existing tuple!";
		it->callback_ = callback_fn;
	}
}

void PairSpace::removeCallback(const std::string& key)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithKey(key);
	if(it == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "No such pair exists. No problem.";
	}
	else
	{
		//SRNP_PRINT_DEBUG << "Removing the callback.";
		it->callback_ = NULL;
	}
}

void PairSpace::addSubscription(const std::string& key, const int& subscriber)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithKey(key);
	if(it == pairs_.end())
	{
		SRNP_PRINT_DEBUG << "Adding subscription (future).";
		Pair new_one (-1, key, "");
		new_one.subscribers_.push_back(subscriber);
		addPair(new_one);
	}
	else
	{
		SRNP_PRINT_DEBUG << "Adding a subscription for " << subscriber;
		if(std::find(it->subscribers_.begin(), it->subscribers_.end(), subscriber) == it->subscribers_.end())
		{
			SRNP_PRINT_DEBUG << "No subscription exists. We add one.";
			it->subscribers_.push_back(subscriber);
		}
		else
		{
			//SRNP_PRINT_DEBUG << "A subscription already exists! Did nothing.";
		}
	}
}

void PairSpace::removeSubscription(const std::string& key, const int& subscriber)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithKey(key);
	if(it == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "No pair exists. No need for cancelling subscription.";
	}
	else
	{
		//SRNP_PRINT_DEBUG << "Cancelling subscription on existing tuple.";
		std::vector <int>::iterator sub_iter = std::find(it->subscribers_.begin(), it->subscribers_.end(), subscriber);
		if(sub_iter == it->subscribers_.end())
		{
			//SRNP_PRINT_DEBUG << "No subscription exists. Nothing done.";
		}
		else
		{
			//SRNP_PRINT_DEBUG << "A subscription exists! Cancelled it.";
			it->subscribers_.erase(sub_iter);
		}
	}
}


void PairSpace::addPair(const Pair& pair)
{
	std::vector<Pair>::iterator iter = getPairIteratorWithKey (pair.getKey());
	if(iter == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "Adding brand NEW PAIR!";
		//SRNP_PRINT_DEBUG << (pair.callback_ != NULL ? "OK HERE": "NULL");
		//printf("Brand new pair added.\n");
		pairs_.push_back(pair);
		pairs_.back().setWriteTime(boost::posix_time::microsec_clock::universal_time());
	}
	else
	{
		//SRNP_PRINT_DEBUG << "UPDATING A PAIR!";
		std::string value = pair.getValue();
		iter->setOwner(pair.getOwner());
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





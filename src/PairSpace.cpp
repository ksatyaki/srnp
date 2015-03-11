/*
  PairSpace.cpp - Provides an implementation of functions defined in
  PairSpace.h
  
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

void PairSpace::addCallbackToAll(Pair::CallbackFunction callback_fn)
{
	
	SRNP_PRINT_DEBUG << "Adding a universal callback to all.";
	u_callback_ = callback_fn;
	for(std::vector <Pair>::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
	{
		iter->callback_ = callback_fn;
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

void PairSpace::addSubscriptionToAll(const int& subscriber)
{
	// If this guy isn't already there in our univ subscribers list.
	if(std::find(u_subscribers_.begin(), u_subscribers_.end(), subscriber) == u_subscribers_.end())
	{
		SRNP_PRINT_INFO << "Adding Universal subscription";
		u_subscribers_.push_back(subscriber);

		for(std::vector <Pair>::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			iter->subscribers_.push_back(subscriber);
		}
			
	}
	else
		SRNP_PRINT_DEBUG << "Got a redundant universal subscription message.";
		
}

void PairSpace::removeSubscriptionToAll(const int& subscriber)
{
	std::vector <int>::iterator it = std::find(u_subscribers_.begin(), u_subscribers_.end(), subscriber);
	// If this guy isn't already there in our univ subscribers list.
	if(it != u_subscribers_.end())
	{
		u_subscribers_.erase(it);

		for(std::vector <Pair>::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			std::vector <int>::iterator pair_sub_iter = std::find (iter->subscribers_.begin(), iter->subscribers_.end(), subscriber);

			if(pair_sub_iter != iter->subscribers_.end())
				iter->subscribers_.erase(pair_sub_iter);
			else
				SRNP_PRINT_ERROR << "Error trying to remove u_subscription. It should exist here...";
		}
			
	}
	else
		SRNP_PRINT_DEBUG << "U SA What U Had to C.";
		
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

		for(std::vector<int>::iterator u_sub_iter = u_subscribers_.begin(); u_sub_iter != u_subscribers_.end(); u_sub_iter++)
			pairs_.back().subscribers_.push_back(*u_sub_iter);

		if(u_callback_)
		{
			pairs_.back().callback_ = u_callback_;
		}
			
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





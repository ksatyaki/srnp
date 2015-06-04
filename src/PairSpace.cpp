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

PairSpace::PairSpace () : cbid_new_(0.00)
{

}

std::pair<int, std::string> PairSpace::getOwnerAndKeyFromCBID(double cbid) {

	std::map<double, std::pair<int, std::string> >::iterator the_one = cbid_to_key_.find(cbid);
	if(the_one == cbid_to_key_.end()) {
		SRNP_PRINT_DEBUG << "DANGER ZONE";
		return std::pair<int, std::string> (-1, "");
	}
	else
	{
		return the_one->second;
	}
}


std::vector <Pair>::iterator PairSpace::getPairIteratorWithOwnerAndKey(const int& owner, const std::string& key)
{
	typedef std::vector <Pair> PairVector;
	for(PairVector::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
	{
		if(iter->getKey().compare(key) == 0 && iter->getOwner() == owner)
		{
			return iter;
		}
	}

	return pairs_.end();
}

CallbackHandle PairSpace::addCallback(const int& owner, const std::string& key, Pair::CallbackFunction callback_fn)
{
	this->cbid_new_ = cbid_new_ + 0.001;
	cbid_to_key_[cbid_new_] = std::pair<int, std::string> (owner, key);
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithOwnerAndKey(owner, key);
	if(it == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "Adding callback (future).";
		Pair new_one (owner, key, "", Pair::INVALID);
		new_one.callbacks_[cbid_new_] = callback_fn;
		addPair(new_one);
	}
	else
	{
		//SRNP_PRINT_DEBUG << "Adding a callback on existing tuple!";
		it->callbacks_[cbid_new_] = callback_fn;
	}

	return cbid_new_;
}

void PairSpace::addCallbackToAll(Pair::CallbackFunction callback_fn)
{
	
	SRNP_PRINT_DEBUG << "Adding a universal callback to all.";
	u_callback_ = callback_fn;

	/*
	for(std::vector <Pair>::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
	{
		iter->callback_ = callback_fn;
	}
	*/
	
}

void PairSpace::removeCallback(const CallbackHandle& cbid)
{
	typedef std::vector <Pair> PairVector;
	std::pair<int, std::string> owner_and_key_of_pair = getOwnerAndKeyFromCBID(cbid);

	if(owner_and_key_of_pair.second.empty()) return;
	
	PairVector::iterator it = getPairIteratorWithOwnerAndKey(owner_and_key_of_pair.first, owner_and_key_of_pair.second);

//SRNP_PRINT_DEBUG << "Removing the callback.";
	std::map<CallbackHandle, Pair::CallbackFunction>::iterator callback_iter = it->callbacks_.find(cbid);
	if(callback_iter == it->callbacks_.end()) {
		SRNP_PRINT_FATAL << "A Callback that is supposed to be here is missing!!!";
	}
	else {
		it->callbacks_.erase(callback_iter);
	}
}

void PairSpace::addSubscription(const int& my_owner, const std::string& key, const int& subscriber)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator itMINE = getPairIteratorWithOwnerAndKey(my_owner, key);
	if(itMINE == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "Adding subscription (future).";
		Pair new_one (my_owner, key, "", Pair::INVALID);
		new_one.subscribers_.push_back(subscriber);
		addPair(new_one);
	}
	else
	{
		//SRNP_PRINT_DEBUG << "Adding a subscription for " << subscriber;
		if(std::find(itMINE->subscribers_.begin(), itMINE->subscribers_.end(), subscriber) == itMINE->subscribers_.end())
		{
			//SRNP_PRINT_DEBUG << "No subscription exists. We add one.";
			itMINE->subscribers_.push_back(subscriber);
		}
		else
		{
			SRNP_PRINT_WARNING << "Got a second subscription message for same pair. Possible bug in code.";
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
		SRNP_PRINT_WARNING << "Got a redundant universal subscription message. Possible bug in code.";
		
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
	else {
	
		for(std::vector <Pair>::iterator iter = pairs_.begin(); iter != pairs_.end(); iter++)
		{
			std::vector <int>::iterator pair_sub_iter = std::find (iter->subscribers_.begin(), iter->subscribers_.end(), subscriber);

			if(pair_sub_iter != iter->subscribers_.end()) {
				//SRNP_PRINT_DEBUG << "SAFE!";
				iter->subscribers_.erase(pair_sub_iter);
			}
				
		}
		
	}	
}


void PairSpace::removeSubscription(const int& my_owner, const std::string& key, const int& subscriber)
{
	typedef std::vector <Pair> PairVector;
	PairVector::iterator it = getPairIteratorWithOwnerAndKey(my_owner, key);
	if(it == pairs_.end())
	{
		SRNP_PRINT_WARNING << "[NO PAIR]: Attempting to cancel a subscription when none exists. Possible bug in code.";
	}
	else
	{
		std::vector <int>::iterator sub_iter = std::find(it->subscribers_.begin(), it->subscribers_.end(), subscriber);
		if(sub_iter == it->subscribers_.end())
		{
			SRNP_PRINT_WARNING << "Attempting to cancel a subscription when none exists. Possible bug in code.";
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
	std::vector<Pair>::iterator iter = getPairIteratorWithOwnerAndKey (pair.getOwner(), pair.getKey());
	if(iter == pairs_.end())
	{
		//SRNP_PRINT_DEBUG << "Adding brand NEW PAIR!";
		//SRNP_PRINT_DEBUG << (pair.callback_ != NULL ? "OK HERE": "NULL");
		//printf("Brand new pair added.\n");
		pairs_.push_back(pair);
		pairs_.back().setWriteTime(boost::posix_time::microsec_clock::universal_time());

		for(std::vector<int>::iterator u_sub_iter = u_subscribers_.begin(); u_sub_iter != u_subscribers_.end(); u_sub_iter++)
			pairs_.back().subscribers_.push_back(*u_sub_iter);
	}
	else
	{
		//SRNP_PRINT_DEBUG << "UPDATING A PAIR!";
		iter->setType(pair.getType());
		std::string value = pair.getValue();
		iter->setOwner(pair.getOwner());
		// Set the value.
		iter->setValue(value);
		// Update time stamp.
		iter->setWriteTime(boost::posix_time::microsec_clock::universal_time());
	}
}

void PairSpace::removePair(const int& owner, const std::string& key)
{
	removePair(getPairIteratorWithOwnerAndKey(owner, key));
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





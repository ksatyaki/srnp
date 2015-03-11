/*
  Pair.h - Defines the basic type used in SRNP - Pair.
  
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

#ifndef PAIR_H_
#define PAIR_H_

#include <string>
#include <vector>
#include <utility>

#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>

namespace srnp
{

/**
 * A Class that derives from PairBase.
 */
class Pair
{
protected:
	/**
	 * The pair object.
	 */
	std::pair <std::string, std::string> pair_;

	/**
	 * The owner id for this pair.
	 */
	int owner_;

	/**
	 * Write time of this Pair.
	 */
	boost::posix_time::ptime write_time_;

	/**
	 * Expiry time.
	 */
	boost::posix_time::ptime expiry_time_;

public:

	typedef boost::function <void(const Pair&)> CallbackFunction;

	/**
	 * A list of owner ids subscribed to this pair.
	 */
	std::vector <int> subscribers_;

	/**
	 * A callback.
	 * There can be only one.
	 */
	CallbackFunction callback_;

	/**
	 * A Mutex.
	 */
	boost::mutex callback_mutex; 
	
	/**
	 * Convenience typedef.
	 */
	typedef boost::shared_ptr <Pair> Ptr;

	Pair ( const int& owner, const std::string& key, const std::string& value) :
		pair_(std::pair <std::string, std::string> (key, value)),
		owner_ (owner)
	{
	}

	inline Pair(const Pair& pair) :
				pair_ (pair.pair_),
				owner_ (pair.owner_),
				write_time_ (pair.write_time_),
		expiry_time_ (pair.expiry_time_),
		subscribers_ (pair.subscribers_),
		callback_ (pair.callback_)
	{
        

    }

	inline Pair& operator = (const Pair& pair)
	{
        pair_ = pair.pair_;
        owner_ = pair.owner_;
        write_time_ = pair.write_time_;
        expiry_time_ = pair.expiry_time_;
		subscribers_ = pair.subscribers_;
		callback_ = pair.callback_;

        return *this;

    }

	Pair(): owner_ (-1)
	{

	}

	/**
	 * Set Key and Value.
	 */
	inline void setPair(const std::string& key, const std::string& value) { pair_.first = key; pair_.second = value; }

	/**
	 * Set the value of the pair.
	 */
	inline void setValue(const std::string& value) { pair_.second = value; }

	/**
	 * Set the owner of the pair.
	 */
	inline void setOwner(const int& owner) { owner_ = owner; }

	/**
	 * Set write time.
	 */
	inline void setWriteTime(const boost::posix_time::ptime& write_time) { write_time_ = write_time; }

	/**
	 * Set expiry time.
	 */
	inline void setExpiryTime(const boost::posix_time::ptime& expiry_time) { expiry_time_ = expiry_time; }

	/**
	 * Get the owner for the pair.
	 */
	inline int getOwner() const { return owner_; }

	/**
	 * Get the key.
	 */
	inline std::string getKey() const { return pair_.first; }

	/**
	 * Get the value.
	 */
	inline std::string getValue() const { return pair_.second; }

	/**
	 * Get a copy of the pair.
	 */
	inline std::pair <std::string, std::string> getPair () const { return pair_; }

	/**
	 * Get write time.
	 */
	inline boost::posix_time::ptime getWriteTime() const { return write_time_; }

	/**
	 * Get expiry time.
	 */
	inline boost::posix_time::ptime getExpiryTime() const { return expiry_time_; }

	/**
	 * The serialization function.
	 */
	template <typename OutputArchive>
	inline void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_;
		o_archive & pair_;
		o_archive & write_time_;
		o_archive & expiry_time_;
	}

};

typedef boost::shared_ptr <Pair> PairPtr;

/**
 * Provide support for cout.
 */
std::ostream& operator<<(std::ostream& s, const Pair& pair);
}
#endif /* PAIR_H_ */

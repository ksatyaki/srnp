#ifndef PAIR_H_
#define PAIR_H_

#include <utility>
#include <string>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/string.hpp>

namespace srnp
{

template <class T>
class Pair
{

protected:
	/**
	 * The owner id for this pair.
	 */
	int owner_;

	/**
	 * The pair object.
	 */
	std::pair <std::string, T> pair_;

public:
	/**
	 * The serialization function.
	 */
	template <typename OutputArchive>
	void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_;
		o_archive & pair_;
	}

	Pair () : owner_(0)
	{

	}

	/**
	 * Constructor must take pair's values.
	 */
	Pair (const std::string& key, const T& value) :
		pair_(std::pair <std::string, T> (key, value)),
		owner_ (0)
	{
	}

	/**
	 * Get a copy of the pair.
	 */
	std::pair <std::string, T> getPair () { return pair_; }

};
}

#endif /* PAIR_H_ */

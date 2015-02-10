#ifndef PAIR_H_
#define PAIR_H_

#include <string>
#include <vector>
#include <utility>

#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

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
	 * A list of owner ids subscribed to this pair.
	 */
	std::vector <int> subscribers_;


public:
	/**
	 * Convenience typedef.
	 */
	typedef boost::shared_ptr <Pair> Ptr;



	Pair (const std::string& key, const std::string& value, const int& owner) :
		pair_(std::pair <std::string, std::string> (key, value)),
		owner_ (owner)
	{
	}

	Pair(): owner_ (-1)
	{

	}

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
	 * The serialization function.
	 */
	template <typename OutputArchive>
	inline void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_;
		o_archive & pair_;
	}

};

typedef boost::shared_ptr <Pair> PairPtr;

/**
 * Provide support for cout.
 */
std::ostream& operator<<(std::ostream& s, const Pair& pair)
{
	s<<"Key: "<<pair.getKey()<<". Value: "<<pair.getValue()<<std::endl;
	return s;
}

}
#endif /* PAIR_H_ */

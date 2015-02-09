#ifndef PAIR_H_
#define PAIR_H_

#include <PairBase.h>
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
template <class T>
class Pair : public PairBase
{
	typedef boost::shared_ptr <Pair <T> > Ptr;
protected:
	/**
	 * The pair object.
	 */
	std::pair <std::string, T> pair_;

public:
	Pair (const std::string& key, const T& value, const int& owner) :
		pair_(std::pair <std::string, T> (key, value)),
		PairBase (owner)
	{
	}

	Pair() : PairBase()
	{

	}

	/**
	 * Get the key.
	 */
	std::string getKey() const { return pair_.first; }

	/**
	 * Get the value.
	 */
	T getValue() const { return pair_.second; }

	/**
	 * Get a copy of the pair.
	 */
	std::pair <std::string, T> getPair () { return pair_; }

	/**
	 * The serialization function.
	 */
	template <typename OutputArchive>
	inline void serialize (OutputArchive& o_archive, const int version)
	{
		o_archive & owner_;
		o_archive & pair_;
	}

	void printOnScreen()
	{
		std::cout<<*this;
	}

};

/**
 * Provide support for cout.
 */
template<typename ValueType>
std::ostream& operator<<(std::ostream& s, const Pair<ValueType>& pair)
{
	s<<"Key: "<<pair.getKey()<<". Value: "<<pair.getValue()<<std::endl;
	return s;
}

// Some convenience typedefs.
typedef Pair <std::string> StringPair;

}
#endif /* PAIR_H_ */

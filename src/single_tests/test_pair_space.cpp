#include <PairSpace.h>

int main()
{
	srnp::PairSpace p;
	srnp::PairBasePtr ptr1 = srnp::PairBasePtr (new srnp::StringPair ("key1", "value1", 123));
	srnp::PairBasePtr ptr2 = srnp::PairBasePtr (new srnp::StringPair ("key2", "value2", 123));
	srnp::PairBasePtr ptr3 = srnp::PairBasePtr (new srnp::StringPair ("key3", "value3", 123));
	srnp::PairBasePtr ptr4 = srnp::PairBasePtr (new srnp::StringPair ("key1", "value432", 123));

	srnp::PairBasePtr ptr5 = srnp::PairBasePtr (new srnp::Pair <int> ("key23", 12, 12));

	p.addPair <std::string> (ptr1);
	p.printPairSpace();
	getchar();
	p.addPair <std::string> (ptr2);
	p.printPairSpace();
	getchar();
	p.addPair <std::string> (ptr3);
	p.printPairSpace();
	getchar();
	p.addPair <std::string> (ptr4);
	p.printPairSpace();
	getchar();

	p.removePair(p.getPairIteratorWithKey <std::string> ("key3"));
	p.printPairSpace();
	getchar();

	p.addPair <std::string> (ptr5);
	p.printPairSpace();
	getchar();

	ptr1.reset();
	ptr2.reset();
	ptr3.reset();
	ptr4.reset();
	ptr5.reset();

	p.printPairSpace();

	return 0;
}

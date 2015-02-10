#include <PairSpace.h>
#include <stdio.h>

int main()
{
	srnp::PairSpace p;
	srnp::PairPtr ptr1 = srnp::PairPtr (new srnp::Pair ("key1", "value1", 123));
	srnp::PairPtr ptr2 = srnp::PairPtr (new srnp::Pair ("key2", "value2", 123));
	srnp::PairPtr ptr3 = srnp::PairPtr (new srnp::Pair ("key3", "value3", 123));
	srnp::PairPtr ptr4 = srnp::PairPtr (new srnp::Pair ("key1", "value432", 123));



	p.addPair(*ptr1);
	p.printPairSpace();
	getchar();
	p.addPair(*ptr2);
	p.printPairSpace();
	getchar();
	p.addPair(*ptr3);
	p.printPairSpace();
	getchar();
	p.addPair(*ptr4);
	p.printPairSpace();
	getchar();

	p.removePair(p.getPairIteratorWithKey("key3"));
	p.printPairSpace();
	getchar();

	ptr1.reset();
	ptr2.reset();
	ptr3.reset();
	ptr4.reset();

	p.printPairSpace();

	return 0;
}

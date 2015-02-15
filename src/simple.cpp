#include <iostream>
#include <boost/asio.hpp>

#include <srnp_kernel.h>

class CallbackForTuple
{
public:
	void callback_function(const srnp::Pair& p, int a)
	{
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << p;
		SRNP_PRINT_DEBUG << "And custom value: " << a;
	}
};

int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup(boost::log::trivial::debug);

	srnp::initialize(argn, args, env);
	sleep(1);

	printf("\nGo!\n");

	srnp::setPair("simple", "test1");
	srnp::setPair("IronMaiden", "test2");
	srnp::setPair("simple.phooler", "test0");
	srnp::setRemotePair(30, "Googler", "Sucker30");
	srnp::setRemotePair(30, "Googler", "Sucker53");
	sleep(1);
	srnp::printSubscribedPairSpace();
	srnp::printPairSpace();

	srnp::registerSubscription(30, "simple.phooler");
	srnp::setPair("simple.phooler", "test1");
	sleep(1);
	srnp::printSubscribedPairSpace();
	srnp::printPairSpace();

	srnp::setPair("simple.phooler", "test2");
	CallbackForTuple callback_object;
	int value = 223423;
	srnp::registerCallback(30, "simple.phooler", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));
	sleep(2);
	srnp::printPairSpace();
	srnp::printSubscribedPairSpace();
	srnp::setPair("simple.phooler", "test3");
	srnp::setPair("simple.phooler", "test4");
	srnp::setPair("simple.phooler", "test5");

	sleep(1);

	srnp::cancelSubscription(30, "simple.phooler");

	sleep(1);
	srnp::setPair("simple.phooler", "test6");

	sleep(1);
	srnp::printPairSpace();
	srnp::printSubscribedPairSpace();

	int i = 20;
	while(i--)
		usleep(500000);

	srnp::shutdown();

	return 0;
}

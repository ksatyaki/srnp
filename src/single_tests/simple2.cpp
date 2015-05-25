#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>

class CallbackForTuple
{
public:
	void callback_function(const srnp::Pair::ConstPtr& p, int a)
	{
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << *p;
		SRNP_PRINT_DEBUG << "And custom value: " << a;
	}
};

int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);
	sleep(1);

	printf("\nGo!\n");

	sleep(1);
	//srnp::printPairSpace();

	srnp::registerSubscription("IronMaiden");
	srnp::registerSubscription("Googler");
	srnp::setPair("simple.phooler", "test1____simple2");
	sleep(1);
	//srnp::printPairSpace();

	srnp::setPair("simple.phooler", "test2___simple2");
	CallbackForTuple callback_object;
	int value = 223423;
	//srnp::registerCallback("Googler", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));
	sleep(2);
	//srnp::printPairSpace();
	srnp::setPair("simple.phooler", "test3___simple2");
	srnp::setPair("simple.phooler", "test4___simple2");
	srnp::setPair("simple.phooler", "test5__simple2");

	sleep(1);

	srnp::cancelSubscription("Googler");

	sleep(1);
	srnp::setPair("simple.phooler", "test6___simple2");

	sleep(1);
	srnp::printPairSpace();

	int i = 20;
	while(i--)
		usleep(500000);

	srnp::shutdown();

	return 0;
}

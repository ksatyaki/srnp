#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>

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
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);
	sleep(1);

	printf("\nGo!\n");

	srnp::setPair("simple", "test1");
	srnp::setPair("IronMaiden", "test2");
	srnp::setPair("simple.phooler", "test0");
	srnp::setPair("Googler", "Sucker30");
	srnp::setPair("Googler", "Sucker53");
	srnp::registerSubscription("simple.phooler");
	srnp::setPair("simple.phooler", "test1_simple");
	srnp::setPair("Googler", "Bonkers");
	srnp::setPair("simple.phooler", "test2_simple");
	CallbackForTuple callback_object;
	int value = 223423;
	srnp::registerCallback("simple.phooler", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));
	//srnp::printPairSpace();
	srnp::setPair("simple.phooler", "test3_simple");
	srnp::setPair("simple.phooler", "test4_simple");
	srnp::setPair("simple.phooler", "test5_simple");

	sleep(1);
	srnp::printPairSpace();

	int i = 20;
	while(i--)
		usleep(500000);

	srnp::shutdown();

	return 0;
}

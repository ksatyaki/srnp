#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>

void callback_function(const srnp::Pair::ConstPtr& p)
{
	SRNP_PRINT_DEBUG << "In callback!";
	SRNP_PRINT_DEBUG << "Tuple: " << *p;
}

int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);
	sleep(1);

	printf("\nGo!\n");

	
	srnp::registerSubscription (1, "simple");
    srnp::registerSubscription (2, "simple");

	/*
	CallbackForTuple callback_object;
	*/
	srnp::CallbackHandle cb1 = srnp::registerCallback(1, "simple", boost::bind(callback_function, _1));
	srnp::CallbackHandle cb2 = srnp::registerCallback(2, "simple", boost::bind(callback_function, _1));
	

	srnp::setPair("simple.phooler", "test6___simple2");

	sleep(1);
	srnp::printPairSpace();

	int i = 20;
	while(i--)
		usleep(500000);

	srnp::shutdown();

	return 0;
}

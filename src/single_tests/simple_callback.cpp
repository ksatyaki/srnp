#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>

class CallbackForTuple
{
public:
	void callback_function(const srnp::Pair::ConstPtr& p)
	{
		SRNP_PRINT_DEBUG << "*****************************************";
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << *p;
		SRNP_PRINT_DEBUG << "*****************************************";
	}
	void back_function(const srnp::Pair::ConstPtr& p)
	{
		SRNP_PRINT_DEBUG << "*****************************************";
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << *p;
		SRNP_PRINT_DEBUG << "*****************************************";
	}
};

int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);
	sleep(1);

	printf("\nGo!\n");

	sleep(1);

	CallbackForTuple callback_object;
	int value = 8;

	srnp::SubscriptionHandle sh1 = srnp::registerSubscription (1, "simple");
	srnp::SubscriptionHandle sh2 = srnp::registerSubscription (2, "simple");
	
	srnp::CallbackHandle cb1 = srnp::registerCallback(1, "simple", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1));
	srnp::CallbackHandle cb2 = srnp::registerCallback(2, "simple", boost::bind(&CallbackForTuple::back_function, &callback_object, _1));

	int i = 20;
	
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	SRNP_PRINT_DEBUG << "One is unregistered.";
	srnp::cancelCallback(cb1);

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		SRNP_PRINT_DEBUG << *(srnp::getPair(1, "simple"));
		SRNP_PRINT_DEBUG << *(srnp::getPair(2, "simple"));
		usleep(500000);
	}

	SRNP_PRINT_DEBUG << "Two is unregistered.";
	srnp::cancelCallback(cb2);

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		SRNP_PRINT_DEBUG << *(srnp::getPair(1, "simple"));
		SRNP_PRINT_DEBUG << *(srnp::getPair(2, "simple"));
		usleep(500000);
	}
	
	srnp::shutdown();
	return 0;
}

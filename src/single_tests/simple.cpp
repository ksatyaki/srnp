#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>

class CallbackForTuple
{
public:
	void callback_function(const srnp::Pair::ConstPtr& p, int a)
	{
		SRNP_PRINT_DEBUG << "*****************************************";
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << *p;
		SRNP_PRINT_DEBUG << "And custom value: " << a;
		SRNP_PRINT_DEBUG << "*****************************************";
	}
};

int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);

	SRNP_PRINT_DEBUG << "GO!";

	sleep(2);

	CallbackForTuple callback_object, co2;
	int value = 8;

	srnp::setPair("Superb.gol", "Goot");
	srnp::SubscriptionHandle sh_1 = srnp::registerSubscription(1, "simple");
	srnp::CallbackHandle cal1 = srnp::registerCallback("simple", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));
	srnp::CallbackHandle cal2 = srnp::registerCallback("simple", boost::bind(&CallbackForTuple::callback_function, &co2, _1, 2));

	int i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::cancelCallback(cal2);
	srnp::cancelSubscription(sh_1);

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::SubscriptionHandle sh_2 = srnp::registerSubscription(1, "simple");
	
	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::cancelSubscription(sh_1);
	srnp::cancelSubscription(sh_2);

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::shutdown();
	return 0;
}

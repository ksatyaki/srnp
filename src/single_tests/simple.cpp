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

	printf("\nGo!\n");

	CallbackForTuple callback_object;
	int value = 8;

	srnp::setPair("Superb.gol", "Goot");
	srnp::registerSubscription("simple");
	srnp::registerCallback("simple", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));

	int i = 30;
	while(i--)
		usleep(500000);

	srnp::shutdown();

	return 0;
}

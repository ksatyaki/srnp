#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>
#include <srnp/meta_tuple_callback.hpp>

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

	CallbackForTuple callback_object;
	int value = 8;
	
	srnp::registerMetaTupleCallback(1, "simple", boost::bind(&CallbackForTuple::callback_function, &callback_object, _1, value));

	int i = 20;
	
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::unregisterMetaTupleCallback(1, "simple");

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}
	
	srnp::shutdown();
	return 0;
}

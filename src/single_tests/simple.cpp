#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>
#include <srnp/meta_pair_callback.hpp>

	void callback_function(const srnp::Pair::ConstPtr& p)
	{
		SRNP_PRINT_DEBUG << "*****************************************";
		SRNP_PRINT_DEBUG << "In callback!";
		SRNP_PRINT_DEBUG << "Tuple: " << *p;
		SRNP_PRINT_DEBUG << "*****************************************";
	}


int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);

	printf("\nGo!\n");

	sleep(1);

	//CallbackForTuple callback_object;
	
	srnp::registerMetaCallback(1, "simple", callback_function);
	
	int i = 20;
	
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}

	srnp::cancelMetaCallback(1, "simple");

	i = 10;
	while(i--) {
		SRNP_PRINT_DEBUG << i;
		usleep(500000);
	}
	
	srnp::shutdown();
	return 0;
}

#include <iostream>
#include <boost/asio.hpp>

#include <srnp/srnp_kernel.h>
#include <srnp/meta_pair_callback.hpp>


int main(int argn, char* args[], char* env[])
{
	srnp::srnp_print_setup("debug");

	srnp::initialize(argn, args, env);

	printf("\nGo!\n");

	sleep(1);
	
	srnp::registerMetaSubscription(1, "simple");
	
	int i = 40;
	
	while(i--) {
		srnp::Pair::ConstPtr pair = srnp::getPairIndirectly(1, "simple");
		if(pair) {
			SRNP_PRINT_INFO << "GOT PAIR!";
			SRNP_PRINT_INFO << "Key: " << pair->getKey() << ", and Value: " << pair->getValue();
		}
		else {
			SRNP_PRINT_INFO << "Nothing yet.";
		}
		usleep(500000);
	}

	srnp::cancelMetaSubscription(1, "simple");

	i = 10;
	while(i--) {
		srnp::Pair::ConstPtr pair = srnp::getPairIndirectly(1, "simple");
		if(pair) {
			SRNP_PRINT_INFO << "GOT PAIR!";
			SRNP_PRINT_INFO << "Key: " << pair->getKey() << ", and Value: " << pair->getValue();
		}
		else {
			SRNP_PRINT_INFO << "Nothing yet.";
		}
		usleep(500000);
	}
	
	srnp::shutdown();
	return 0;
}

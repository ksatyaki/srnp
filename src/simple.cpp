#include <iostream>
#include <boost/asio.hpp>

#include <srnp/client.h>
#include <srnp/server.h>

#include <Pair.h>

int main(int argn, char* args[])
{
	if(argn < 5)
	{
		printf("Needs port, owner and friend's port as arguments.\n");
		return 0;
	}

	boost::asio::io_service io_;
	printf("\nstrats!\n");

	std::queue <srnp::Pair> q;
	int owner = 332;

	srnp::Server server (io_, atoi(args[1]), atoi(args[2]), q);

	std::pair <std::string, std::string> host_pair ("127.0.0.1", args[2]);
	std::pair <std::string, std::string> host_pair2 (args[3], args[4]);
	std::vector < std::pair <std::string, std::string> > vec_host_pairs;
	vec_host_pairs.push_back(host_pair);
	vec_host_pairs.push_back(host_pair2);

	srnp::Client cli (io_, owner, vec_host_pairs, q);

	printf("\nGo!\n");

	cli.setPair("simple", "test");
	sleep(1);
	server.printPairSpace();

	cli.setPair("IronMaiden", "test");
	sleep(1);
	server.printPairSpace();

	cli.setPair("simple.phooler", "test");
	sleep(1);
	server.printPairSpace();

	while(1)
		sleep(1);

	return 0;
}

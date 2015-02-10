#include <iostream>
#include <boost/asio.hpp>

#include <srnp/client.h>
#include <srnp/server.h>

#include <Pair.h>

int main()
{
	boost::asio::io_service io_;
	printf("\nstrats!\n");

	std::queue <srnp::Pair> q;
	int owner = 332;

	srnp::Server server (io_, 332, 33133, q);

	//std::pair <std::string, std::string> host_pair ("127.0.0.1", "33133");
	std::vector < std::pair <std::string, std::string> > vec_host_pairs;
	//vec_host_pairs.push_back(host_pair);

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

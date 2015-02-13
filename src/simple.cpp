#include <iostream>
#include <boost/asio.hpp>

#include <srnp/client.h>
#include <srnp/server.h>
#include <srnp/srnp_print.h>

#include <Pair.h>

int main(int argn, char* args[])
{
	srnp::srnp_print_setup(boost::log::trivial::debug);

	if(argn < 4)
	{
		printf("\n<master_ip> <master_port> <owner_id>");
		return 0;
	}

	boost::asio::io_service io_;
	printf("\nstrats!\n");

	std::queue <srnp::Pair> q;
	int owner = 332;

	std::string master_hub_ip = args[1];
	std::string master_hub_port = args[2];
	srnp::Server server (io_, master_hub_ip, master_hub_port, q, atoi(args[3]));

	unsigned short port = server.getPort();

	std::stringstream sport;
	sport << port;

	printf("\n********");
	printf("\nINFO ALL");
	printf("\n********");

	printf("\nPORT INT: %d", server.getPort());
	printf("\nPORT STR: %s", sport.str().c_str());
	printf("\nOWNER: %d", server.owner());
	srnp::Client cli (io_, "127.0.0.1", sport.str(), q);

	sleep(1);

	printf("\nGo!\n");

	cli.setPair("simple", "test1");
	cli.setPair("IronMaiden", "test2");
	cli.setPair("simple.phooler", "test3");
	cli.setPair(30, "Googler", "Sucker30");
	cli.setPair(53, "Googler", "Sucker53");
	sleep(1);
	server.printPairSpace();

	cli.setPair("simple.phooler", "tests");
	cli.setPair(30, "Googler", "Sucker2");
	cli.setPair(53, "Googler", "Sucker2");
	sleep(1);
	server.printPairSpace();


	printf("\nAll over.");
	io_.run();

	return 0;
}

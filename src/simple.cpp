#include <iostream>
#include <boost/asio.hpp>

#include <srnp/client.h>
#include <srnp/server.h>

#include <Pair.h>

int main(int argn, char* args[])
{

	if(argn < 4)
	{
		printf("\nWrong.");
		return 0;
	}

	boost::asio::io_service io_server;
	boost::asio::io_service io_client;
	printf("\nstrats!\n");

	std::queue <srnp::Pair> q;
	int owner = 332;

	std::string master_hub_ip = args[1];
	std::string master_hub_port = args[2];
	srnp::Server server (io_server, "0.0.0.0", master_hub_ip, master_hub_port, q);

	unsigned short port = server.getPort();

	std::stringstream sport;
	sport << port;

	printf("\n********");
	printf("\nINFO ALL");
	printf("\n********");

	printf("\nPORT INT: %d", server.getPort());
	printf("\nPORT STR: %s", sport.str().c_str());
	printf("\nOWNER: %d", server.owner());
	srnp::Client cli (io_client, args[3], sport.str(), q);

	printf("\nGo!\n");

	/*
	cli.setPair("simple", "test");
	sleep(1);
	server.printPairSpace();

	cli.setPair("IronMaiden", "test");
	sleep(1);
	server.printPairSpace();

	cli.setPair("simple.phooler", "test");
	sleep(1);
	server.printPairSpace();
	*/

	printf("\nAll over.");
	io_client.run();

	return 0;
}

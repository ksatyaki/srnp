#include <iostream>
#include <boost/asio.hpp>

#include <srnp/client.h>
#include <srnp/server.h>

#include <Pair.h>

int main(int argn, char* args[])
{

	if(argn < 3)
	{
		printf("\nWrong.");
		return 0;
	}

	boost::asio::io_service io_;
	printf("\nstrats!\n");

	std::queue <srnp::Pair> q;
	int owner = 332;

	std::string master_hub_ip = args[1];
	std::string master_hub_port = args[2];
	srnp::Server server (io_, master_hub_ip, master_hub_port, q);

	unsigned short port = server.getPort();

	printf("\n********");
	printf("\nINFO ALL");
	printf("\n********");

	printf("\nPORT: %d", server.getPort());
	printf("\nOWNER: %d", server.owner());

	std::stringstream ss;
	ss << port;
	srnp::Client cli (io_, "127.0.0.1", ss.str(), q);

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

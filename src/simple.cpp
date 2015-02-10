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

	srnp::Server server (io_, q);

	unsigned short port = server.getPort();

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

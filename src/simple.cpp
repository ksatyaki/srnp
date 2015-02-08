#include <iostream>
#include <boost/asio.hpp>

#include <srnp/server.h>

int main()
{
	boost::asio::io_service io_;
	printf("\nstrats!\n");
	srnp::Server server (io_, 33133);

	printf("\nGo!\n");
	server.waitForEver();

	return 0;
}

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <ctime>
#include <boost/bind.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <iomanip>

#include <MessageHeader.h>
#include <Pair.h>

#ifdef WIN32
#include <windows.h>
#endif 

#ifdef UNIX
#include <unistd.h>
#include <sys/types.h>
#endif

using boost::asio::ip::tcp;

class TestClient
{
	/** 
	  * This is the io_service object needed.
	  */
	boost::asio::io_service io_service_;

	/**
	  * A Resolver to resolve requests/queries from the client.
	  */
	tcp::resolver resolver_;
	/**
	  * A Socket.
	  */
	boost::shared_ptr<tcp::socket> socket_;

	std::string out_header_size_;

	std::string out_header_;

	std::string out_data_;

public:
	TestClient();

	/**
	  * A client function. This requests the daytime.
	  */
	void testServer(const std::string& host);
};

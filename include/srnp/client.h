/*
 * client.h
 *
 *  Created on: Feb 8, 2015
 *      Author: ace
 */

#ifndef SRNP_CLIENT_H_
#define SRNP_CLIENT_H_

#include <queue>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
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

#define RECONNECT_TIMEOUT 10

using boost::asio::ip::tcp;

namespace srnp
{

class ClientSession
{
protected:

	/**
	 * Save the endpoint_iterator for this session.
	 */
	tcp::resolver::iterator endpoint_iterator_;

	/**
	 * A deadline timer to wait for timeout before attempting to reconnect.
	 */
	boost::asio::deadline_timer reconnect_timer_;

	/**
	 * A Resolver to resolve requests/queries from the client.
	 */
	tcp::resolver resolver_;

	/**
	 * A socket per session.
	 */
	boost::shared_ptr<tcp::socket> socket_;

	/**
	 * A handler to see if we are connected.
	 */
	void handleConnection(const boost::system::error_code& err);

	/**
	 * This function is called when reconnection should be attempted.
	 * That is, the timer expires and this gets called.
	 */
	void reconnectTimerCallback();

public:

	/**
	 * The setPair sets a pair on the given connection.
	 */
	bool sendPair(const std::string& out_header_size, const std::string& out_header, const std::string& out_data);



	ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port);
};

class Client
{
protected:

	int owner_id_;

	std::vector <boost::shared_ptr <ClientSession> > client_sessions_;

	boost::shared_ptr <ClientSession> my_server_session_;

	boost::asio::io_service& service_;

	boost::asio::deadline_timer heartbeat_timer_;

	boost::posix_time::time_duration elapsed_time_;

	void onHeartbeat();

	std::queue <Pair>& pair_queue_;

public:

	bool setPair(const std::string& key, const std::string& value);

	Client(boost::asio::io_service& service, const int& owner_id, const std::vector< std::pair <std::string, std::string> >& servers, std::queue <Pair>& pair_queue);

	virtual ~Client();
};

} /* namespace srnp */

#endif /* INCLUDE_SRNP_CLIENT_H_ */

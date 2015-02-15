/*
 * client.h
 *
 *  Created on: Feb 8, 2015
 *      Author: ace
 */

#ifndef SRNP_CLIENT_H_
#define SRNP_CLIENT_H_

#include <srnp/srnp_print.h>

#include <queue>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <CommMessages.h>
#include <MessageHeader.h>
#include <MasterMessages.h>
#include <Pair.h>
#include <PairQueue.h>

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

class Client;

class ClientSession
{
protected:

	/**
	 * To know if this is our session with our own server.
	 */
	bool is_this_our_server_session_;

	/**
	 * To receive the size.
	 */
	boost::array <char, sizeof(size_t)> in_size_;

	/**
	 * To receive the header.
	 */
	std::vector <char> in_header_;

	/**
	 * To receive data.
	 */
	std::vector <char> in_data_;

	/**
	 * Save the endpoint_iterator for this session.
	 */
	tcp::resolver::iterator endpoint_iterator_;

	/**
	 * A deadline timer to wait for timeout before attempting to reconnect.
	 */
	boost::asio::deadline_timer reconnect_timer_;

	/**
	 * A Resolver to resolve requests/queries.
	 */
	tcp::resolver resolver_;

	/**
	 * A socket per session.
	 */
	boost::shared_ptr<tcp::socket> socket_;

	/**
	 * A handler to see if we are connected.
	 */
	void handleConnection (Client* client, const boost::system::error_code& err);

	/**
	 * Handle MasterMessage and UpdateComponents messages from Server.
	 * Pair messages also arrive here. We send it to the corresponding server.
	 */
	void handleMMandUCandPairMsgs (Client* client, const boost::system::error_code& error);

	/**
	 * This function is called when reconnection should be attempted.
	 * That is, the timer expires and this gets called.
	 */
	void reconnectTimerCallback(Client* client);

	Client* client_;

	bool setPairUpdate(const Pair& pair, Client* client);

public:

	/**
	 * Only one guy should write to this socket at any time.
	 */
	boost::mutex server_write_mutex;

	/**
	 * Sends the message to the server. Async operation.
	 */
	bool sendDataToServer(const std::string& out_header_size, const std::string& out_header, const std::string& out_data);

	ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port, bool is_this_our_server_session = false, Client* client = NULL);
};

class Client
{

protected:

	boost::mutex socket_write_mutex;

	friend class ClientSession;

	int owner_id_;

	boost::shared_ptr <ClientSession> my_server_session_;

	boost::asio::io_service& service_;

	boost::posix_time::time_duration elapsed_time_;

	PairQueue& pair_queue_;

	std::map <int, ClientSession*> sessions_map_;

public:

	bool setPair(const std::string& key, const std::string& value);
	bool setPair(const int& owner, const std::string& key, const std::string& value);

	bool registerCallback(const int& owner, const std::string& key, Pair::CallbackFunction callback_fn);
	bool cancelCallback(const int& owner, const std::string& key);

	void registerSubscription (const int& owner, const std::string& key);
	void cancelSubscription (const int& owner, const std::string& key);

	Client(boost::asio::io_service& service, std::string our_server_ip, std::string our_server_port, PairQueue& pair_queue);

	virtual ~Client();
};

} /* namespace srnp */

#endif /* INCLUDE_SRNP_CLIENT_H_ */

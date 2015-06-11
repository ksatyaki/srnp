/*
  client.h - Client is what an application directly uses.
  
  Copyright (C) 2015  Chittaranjan Srinivas Swaminathan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#ifndef SRNP_CLIENT_H_
#define SRNP_CLIENT_H_

#include <srnp/srnp_print.h>

#include <queue>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <string.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MessageHeader.h>
#include <srnp/msgs/MasterMessages.h>
#include <srnp/Pair.h>
#include <srnp/PairQueue.h>
#include <srnp/PairSpace.h>

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
	boost::array <char, sizeof(uint64_t)> in_size_;

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
	 * The owner id of the endpoint srnp component.
	 */
	int endpoint_owner_id_;

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

	bool setPairUpdate(const Pair& pair, Client* client, int subscriber_only_one);

	void sendSubscriptionMsgs(Client* client);

public:

	/**
	 * Only one guy should write to this socket at any time.
	 */
	boost::mutex server_write_mutex;

	/**
	 * Sends the message to the server. Async operation.
	 */
	bool sendDataToServer(const std::string& out_header_size, const std::string& out_header, const std::string& out_data); 

	ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port, bool is_this_our_server_session = false, Client* client = NULL, const int& endpoint_owner_id = -10);
	~ClientSession();
};

class Client
{

protected:

	std::vector <std::string> subscribed_tuples_;

	std::map <int, std::vector <std::string> > owner_id_to_subscribed_pairs_;

	std::map <SubscriptionHandle, std::pair <int, std::string> > subscription_handle_to_owner_key_;

	std::map <SubscriptionHandle, std::string> subscription_handle_to_key_multiple_;

	boost::mutex socket_write_mutex;

	friend class ClientSession;

	int owner_id_;

	bool ready_;

	boost::shared_ptr <ClientSession> my_server_session_;

	boost::asio::io_service& service_;

	boost::posix_time::time_duration elapsed_time_;

	PairQueue& pair_queue_;

	PairSpace& pair_space_;

	std::map <int, ClientSession*> sessions_map_;

	SubscriptionHandle subscription_handle_new_ ;

public:
	inline bool ready() { return ready_; }
	bool setPair(const std::string& key, const std::string& value, const Pair::PairType& type = Pair::STRING);
	bool setRemotePair(const int& owner, const std::string& key, const std::string& value, const Pair::PairType& type = Pair::STRING);
	bool setPairIndirectly(const int& metaowner, const std::string& metakey, const std::string& value);
	
	bool setMetaPair(const int& meta_owner, const std::string& meta_key, const int& owner, const std::string& key);
	bool initMetaPair(const int& meta_owner, const std::string& meta_key);

	Pair::ConstPtr getPair(const int& owner, const std::string& key);
	Pair::ConstPtr getPairIndirectly(const int& metaowner, const std::string& metakey);

	CallbackHandle registerCallback(const int& owner, const std::string& key, const Pair::CallbackFunction& callback_fn);
	void cancelCallback(const CallbackHandle& cbid);

	SubscriptionHandle registerSubscription (const int& owner, const std::string& key);
	SubscriptionHandle registerSubscription (const std::string& key);

	void cancelSubscription (const SubscriptionHandle &handle);
	void cancelSubscription (const int& owner, const std::string& key);
	void cancelSubscription (const std::string& key);

	Client(boost::asio::io_service& service, std::string our_server_ip, std::string our_server_port, PairSpace& pair_space, PairQueue& pair_queue);

	virtual ~Client();
};

std::vector<std::string> extractStrings(const char p[]);

} /* namespace srnp */

#endif /* INCLUDE_SRNP_CLIENT_H_ */

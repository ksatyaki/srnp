/*
  server.h - Server sits behind the client and takes care of async-
  things.
  
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

#ifndef SRNP_SERVER_H_
#define SRNP_SERVER_H_

#include <srnp/srnp_print.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <string>
#include <vector>
#include <queue>

#include <srnp/msgs/CommMessages.h>
#include <srnp/msgs/MasterMessages.h>
#include <srnp/msgs/MessageHeader.h>
#include <srnp/Pair.h>
#include <srnp/PairSpace.h>
#include <srnp/PairQueue.h>

using boost::asio::ip::tcp;

namespace srnp
{

class Server;
class ServerSession;

class MasterLink
{
	/**
	 *  Save a copy of the initial master message.
	 */
	MasterMessage mm_;

	boost::array <char, sizeof(size_t)> in_size_;

	std::string master_ip_;

	std::string master_port_;

	std::vector <char> in_data_;

	tcp::socket socket_;

	boost::shared_ptr <ServerSession>& my_client_session_;

	tcp::resolver resolver_;

	void handleUpdateComponentsMsg(const boost::system::error_code& e);

	void indicatePresence(Server* server, int DesiredOwnerId);

public:
	void sendMMToOurClientAndWaitForUCMsg();

	MasterLink(boost::asio::io_service& service, std::string master_ip, std::string master_port, boost::shared_ptr <ServerSession>& my_client_session, Server* server, int desired_owner_id);
};

class ServerSession
{
	Server* server_;
	
	boost::mutex socket_write_mutex;

	int& owner_;

	PairSpace& pair_space_;

	PairQueue& pair_queue_;

	tcp::socket socket_;

	std::string out_msg_;

	std::string out_size_;

	std::string out_header_;

	boost::array <char, sizeof(size_t)> in_header_size_buffer_;

	std::vector <char> in_header_buffer_;

	std::vector <char> in_data_buffer_;

	void handleReadHeaderSize(const boost::system::error_code& e);

	void handleReadHeader(const boost::system::error_code& e);

	void handleReadPairUpdate(const boost::system::error_code& e);

	void handleReadSubscription(const boost::system::error_code& e);

	void handleReadPair(const boost::system::error_code& e);

	void handleWrite(const boost::system::error_code& e);

	void sendPairUpdateToClient(const Pair& to_up, int sub_only_one = -1);

	bool sendDataToClient(const std::string& out_header_size, const std::string& out_header, const std::string& out_data);


public:

	static int session_counter;

	ServerSession (boost::asio::io_service& service, PairSpace& pair_space, PairQueue& pair_queue, int& owner, Server* server);

	~ServerSession ();

	boost::system::error_code sendMasterMsgToOurClient(MasterMessage msg);

	boost::system::error_code sendUpdateComponentsMsgToOurClient(UpdateComponents msg);

	inline tcp::socket& socket() { return socket_; }

	inline void startReading()
	{
		boost::asio::async_read(socket_, boost::asio::buffer(in_header_size_buffer_), boost::bind(&ServerSession::handleReadHeaderSize, this, boost::asio::placeholders::error) );
	};

};

class Server
{
protected:

	std::string master_ip_, master_port_;

	int owner_id_;

	boost::shared_ptr <MasterLink> my_master_link_;

	unsigned short port_;

	boost::asio::io_service& io_service_;

	boost::asio::deadline_timer heartbeat_timer_;

	boost::asio::io_service::strand strand_;

	tcp::acceptor acceptor_;

	boost::thread spin_thread_[4];

	boost::shared_ptr<ServerSession> my_client_session_;

	void handleAcceptedMyClientConnection(boost::shared_ptr<ServerSession>& client_session, int desired_owner_id, const boost::system::error_code& e);

	void handleAcceptedConnection(ServerSession* new_session, const boost::system::error_code& e);

	void onHeartbeat();

	boost::posix_time::time_duration elapsed_time_;

	PairQueue& pair_queue_;

	PairSpace& pair_space_;

public:

	inline unsigned short getPort() { return port_; };

	inline int& owner() { return owner_id_; }

	inline ServerSession* my_client_session() { return my_client_session_.get(); };

	inline void printPairSpace() { pair_space_.printPairSpace(); }

	Server(boost::asio::io_service& service, std::string master_ip, std::string master_port, PairSpace& pair_space, PairQueue& pair_queue, int desired_owner_id = -1);

	void startSpinThreads();

	void waitForEver();

	virtual ~Server();
};

} /* namespace srnp */

#endif /* SRNP_SERVER_H_ */

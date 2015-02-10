/*
 * server.h
 *
 *  Created on: Jan 13, 2015
 *      Author: ace
 */

#ifndef SRNP_SERVER_H_
#define SRNP_SERVER_H_

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <string>
#include <vector>
#include <queue>

#include <MessageHeader.h>
#include <Pair.h>
#include <PairSpace.h>

using boost::asio::ip::tcp;

namespace srnp
{

class ServerSession
{
	PairSpace& pair_space_;

	std::queue <Pair>& pair_queue_;

	tcp::socket socket_;

	std::string out_data_;

	std::string out_header_;

	boost::array <char, sizeof(size_t)> in_header_size_buffer_;

	std::vector <char> in_header_buffer_;

	std::vector <char> in_data_buffer_;

	void handleReadHeaderSize(const boost::system::error_code& e);

	void handleReadHeader(const boost::system::error_code& e);

	void handleReadData(const boost::system::error_code& e);

	void handleWrite(const boost::system::error_code& e);


public:

	static int session_counter;

	ServerSession (boost::asio::io_service& service, PairSpace& pair_space, std::queue <Pair>& pair_queue);

	~ServerSession ();

	inline tcp::socket& socket() { return socket_; }

	void startReading();

};

class Server
{
protected:

	int owner_id_;

	std::queue <Pair>& pair_queue_;

	boost::asio::io_service& io_service_;

	boost::asio::deadline_timer heartbeat_timer_;

	boost::asio::io_service::strand strand_;

	tcp::acceptor acceptor_;

	boost::thread spin_thread_[4];

	void handleAcceptedConnection(ServerSession* new_session, const boost::system::error_code& e);

	void onHeartbeat();

	boost::posix_time::time_duration elapsed_time_;

	PairSpace pair_space_;

public:

	inline void printPairSpace() { pair_space_.printPairSpace(); }

	Server(boost::asio::io_service& service, const int& owner_id, const int& port, std::queue <Pair>& pair_queue);

	void startSpinThreads();

	void waitForEver();

	virtual ~Server();
};

} /* namespace srnp */

#endif /* SRNP_SERVER_H_ */

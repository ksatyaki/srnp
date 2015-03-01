/*
 * master_hub.h
 *
 *  Created on: Feb 10, 2015
 *      Author: ace
 */

#ifndef MASTER_HUB_H_
#define MASTER_HUB_H_

#include <srnp/srnp_print.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <string>
#include <vector>
#include <queue>

#include <srnp/msgs/MasterMessages.h>

using boost::asio::ip::tcp;

namespace srnp
{
class MasterHub;
class MasterHubSession
{
	/**
	 * This is the owner id of the component connected to this session.
	 */
	int owner_;

	tcp::socket socket_;

	std::string out_data_;

	std::string out_header_;

	std::vector <char> in_data_;

	boost::array <char, sizeof(size_t)> in_buffer_;

	boost::array <char, sizeof(size_t)> in_size_;

public:
	void handleRead(MasterHub* master, const boost::system::error_code& e);

	MasterHubSession (boost::asio::io_service& service);

	~MasterHubSession ();

	inline std::vector <char>& in_data() { return in_data_; }

	inline boost::array <char, sizeof(size_t)>& in_buffer() { return in_buffer_; }

	inline boost::array <char, sizeof(size_t)>& in_size() { return in_size_; }

	inline int getOwner() { return owner_; }

	inline tcp::socket& socket() { return socket_; }

	inline void setOwner (int own) { owner_ = own; }

	void handleAsyncWriteMsg(const boost::system::error_code& ec);

	inline void sendAsyncMsg(std::string data)
	{
		try {
		boost::asio::async_write(socket_, boost::asio::buffer(data), boost::bind(&MasterHubSession::handleAsyncWriteMsg, this, boost::asio::placeholders::error));
		} catch (std::exception& ex) {
			SRNP_PRINT_FATAL << "We got exception: " << ex.what();
		}
	}


};

class MasterHub
{
	boost::asio::strand strand_;

	std::map <int, MasterHubSession*> sessions_map_;

	std::map <int, std::string> ports_map_;

	boost::asio::io_service& io_service_;

	boost::asio::deadline_timer heartbeat_timer_;

	tcp::acceptor acceptor_;

	boost::thread spin_thread_[2];

	void startSpinThreads();

	void onHeartbeat();

	void handleAcceptedConnection(MasterHubSession* new_session, const boost::system::error_code& e);

public:
	inline boost::asio::strand& strand() { return strand_; }

	void sendUpdateComponentsMessageToAll(UpdateComponents msg);

	void sendMasterMessageToComponent(MasterHubSession* new_session, MasterMessage msg);

	inline std::map <int, MasterHubSession*>& sessions_map() { return sessions_map_;}

	static boost::random::mt19937 gen;

	static int buss;

	static int makeNewOwnerId(std::string port_no);

	MasterHub(boost::asio::io_service& service, unsigned short port);

	virtual ~MasterHub();
};

} /* namespace srnp */

#endif /* MASTER_HUB_H_ */

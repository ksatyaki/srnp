/*
 * master.h
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#ifndef INCLUDE_SRNP_MASTER_H_
#define INCLUDE_SRNP_MASTER_H_

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <string>
#include <vector>
#include <queue>

#include <MasterMessages.h>

using boost::asio::ip::tcp;

namespace srnp
{

/**
 * Forward declaration
 */
 class Master;

class MasterSession
{
	 friend class Master;

	/**
	 * This is the owner id of the component connected to this session.
	 */
	int owner_;

	tcp::socket socket_;

	std::string out_data_;

	std::string out_header_;

	boost::array <char, sizeof(size_t)> in_buffer_;

	void handleRead(Master* master, const boost::system::error_code& e);


public:

	inline void setOwner (int own) { owner_ = own; }

	inline boost::system::error_code sendSyncMsg(std::string data)
	{
		boost::system::error_code error;
		boost::asio::write(socket_, boost::asio::buffer(data), error);
		return error;
	}

	MasterSession (boost::asio::io_service& service);

	~MasterSession ();

	inline int getOwner() { return owner_; }

	inline tcp::socket& socket() { return socket_; }

};

class Master
{
	friend class MasterSession;

	std::map <int, MasterSession*> sessions_map_;

	boost::asio::io_service& io_service_;

	boost::asio::deadline_timer heartbeat_timer_;

	tcp::acceptor acceptor_;

	void handleAcceptedConnection(MasterSession* new_session, const boost::system::error_code& e);

	void sendUpdateComponentsMessageToAll(UpdateComponents msg);

	void sendMasterMessageToComponent(MasterSession* new_session, MasterMessage msg);

	void onHeartbeat();

	boost::posix_time::time_duration elapsed_time_;

public:

	/**
	 * A static pseudo-random number generator.
	 */
	static boost::random::mt19937 gen;

	static int makeNewOwnerId();

	Master(boost::asio::io_service& service, int port);

	virtual ~Master();
};

} /* namespace srnp */

#endif /* INCLUDE_SRNP_MASTER_H_ */

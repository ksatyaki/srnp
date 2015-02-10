/*
 * master.cpp
 *
 *  Created on: Feb 9, 2015
 *      Author: ace
 */

#include <srnp/master.h>

namespace srnp {

boost::random::mt19937 Master::gen;

MasterSession::MasterSession (boost::asio::io_service& service) :
		socket_ (service)
{

}

MasterSession::~MasterSession ()
{
	socket_.close();
}

void MasterSession::handleRead(Master* master, const boost::system::error_code& e)
{
	if(!e)
	{
		printf("\nGod! We received garbage data. Something is fishy...");
		boost::asio::async_read(this->socket_, boost::asio::buffer(this->in_buffer_), boost::bind(&MasterSession::handleRead, this, master, boost::asio::placeholders::error) );
	}
	else
	{
		master->sessions_map_.erase(owner_);

		// Create a component info message and send to all with a delete message.
		UpdateComponents update_msg;
		update_msg.component.ip = socket_.remote_endpoint().address().to_string();
		update_msg.component.owner = owner_;
		update_msg.component.port = socket_.remote_endpoint().port();
		update_msg.operation = UpdateComponents::DELETE;

		master->sendUpdateComponentsMessageToAll(update_msg);

		printf("\nSession Collapse...");
		delete this;
	}

}


Master::Master(boost::asio::io_service& service, unsigned short port) :
		io_service_ (service),
		acceptor_ (service, tcp::endpoint(tcp::v4(), port)),
		heartbeat_timer_ (service, boost::posix_time::seconds(1))
{
	MasterSession* new_session = new MasterSession(io_service_);

	acceptor_.async_accept (new_session->socket(), boost::bind(&Master::handleAcceptedConnection, this, new_session, boost::asio::placeholders::error));
	// Register a callback for the timer. Called ever second.
	heartbeat_timer_.async_wait (boost::bind(&Master::onHeartbeat, this));
}

void Master::sendUpdateComponentsMessageToAll(UpdateComponents msg)
{
	for(std::map <int, MasterSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
	{

		std::ostringstream msg_stream;
		boost::archive::text_oarchive msg_archive(msg_stream);
		msg_archive << msg;
		std::string out_msg_ = msg_stream.str();
		// END

		// Prepare header length
		std::ostringstream size_stream;
		size_stream << std::setw(sizeof(size_t)) << std::hex << out_msg_.size();
		if (!size_stream || size_stream.str().size() != sizeof(size_t))
		{
			// Something went wrong, inform the caller.
			/*
				boost::system::error_code error(boost::asio::error::invalid_argument);
				socket_.io_service().post(boost::bind(handler, error));
				return;

			 */
		}
		std::string out_size_ = size_stream.str();

		boost::system::error_code error;

		// Write the serialized data to the socket. We use "gather-write" to send
		// both the header and the data in a single write operation.

		error = (iter->second)->sendSyncMsg(out_size_);
		printf("\nDone writing Size. Error: %s.", error.message().c_str());
		error = (iter->second)->sendSyncMsg(out_msg_);
		printf("\nDone writing Data. Error: %s.", error.message().c_str());

	}
}

void Master::sendMasterMessageToComponent(MasterSession *new_session, MasterMessage msg)
{
	std::ostringstream msg_stream;
	boost::archive::text_oarchive msg_archive(msg_stream);
	msg_archive << msg;
	std::string out_msg_ = msg_stream.str();
	// END

	// Prepare header length
	std::ostringstream size_stream;
	size_stream << std::setw(sizeof(size_t)) << std::hex << out_msg_.size();
	if (!size_stream || size_stream.str().size() != sizeof(size_t))
	{
		// Something went wrong, inform the caller.
		/*
			boost::system::error_code error(boost::asio::error::invalid_argument);
			socket_.io_service().post(boost::bind(handler, error));
			return;

		 */
	}
	std::string out_size_ = size_stream.str();

	boost::system::error_code error;

	// Write the serialized data to the socket. We use "gather-write" to send
	// both the header and the data in a single write operation.

	error = new_session->sendSyncMsg(out_size_);
	printf("\nDone writing Size. Error: %s.", error.message().c_str());
	error = new_session->sendSyncMsg(out_msg_);
	printf("\nDone writing Data. Error: %s.", error.message().c_str());

}

void Master::onHeartbeat()
{
	// TODO: SEE IF EVERYTHING IS OK BEFORE DOING THIS!
	elapsed_time_ += boost::posix_time::seconds(1);
	heartbeat_timer_.expires_at(heartbeat_timer_.expires_at() + boost::posix_time::seconds(1));
	heartbeat_timer_.async_wait (boost::bind(&Master::onHeartbeat, this));
	printf("\n*********************************************************");
	printf("\nElapsed time: "); std::cout << elapsed_time_ << std::endl;
	printf("\nAcceptor State: %s", acceptor_.is_open() ? "Open" : "Closed");
	printf("\nNo. of Active Sessions: %d", sessions_map_.size());
	printf("\n*********************************************************\n");
}

int Master::makeNewOwnerId()
{
	boost::random::uniform_int_distribution<> dist(1, 1000);
	return dist(gen);
}

void Master::handleAcceptedConnection (MasterSession* new_session, const boost::system::error_code& e)
{
	if(!e)
	{
		// Send this guy his owner_id. And all components we have.
		MasterMessage msg;

		// Generate unique id for this guy.
		bool unique = true;
		while(!unique)
		{
			msg.owner = makeNewOwnerId();
			bool broke = false;
			for(std::map <int, MasterSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
			{
				if(iter->first == msg.owner)
				{
					unique = false;
					broke = true;
					break;
				}
			}

			if(!broke)
				break;
		}

		new_session->setOwner (msg.owner);

		// Collect component info to send to this new guy.
		for(std::map <int, MasterSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
		{
			ComponentInfo info;
			info.ip = (iter->second)->socket().remote_endpoint().address().to_string();
			info.owner = (iter->first);
			info.port = (iter->second)->socket().remote_endpoint().port();

			msg.all_components.push_back(info);
		}

		// Send master message for this guy.
		sendMasterMessageToComponent(new_session, msg);

		// Create a component info message and send to all with a add message.
		UpdateComponents update_msg;
		update_msg.component.ip = new_session->socket().remote_endpoint().address().to_string();
		update_msg.component.owner = new_session->getOwner();
		update_msg.component.port = new_session->socket().remote_endpoint().port();
		update_msg.operation = UpdateComponents::ADD;

		sendUpdateComponentsMessageToAll(update_msg);

		// Finally add this guy to the HashMap.
		sessions_map_[new_session->getOwner()] = new_session;


		printf("\n[In MasterServer::handleAcceptedConnection]: We got error: %s.\n", e.message().c_str());

		boost::asio::async_read(new_session->socket(), boost::asio::buffer(new_session->in_buffer_), boost::bind(&MasterSession::handleRead, new_session, this, boost::asio::placeholders::error) );

		MasterSession* new_session_ = new MasterSession(io_service_);
		acceptor_.async_accept (new_session_->socket(), boost::bind(&Master::handleAcceptedConnection, this, new_session_, boost::asio::placeholders::error));
	}
	else
	{
		printf("\nSession Collapses...");
		delete new_session;
	}
}

Master::~Master()
{

}

} /* namespace srnp */

int main()
{
	boost::asio::io_service service;
	unsigned short port = 33133;
	srnp::Master (service, port);

	service.run();

	return 0;

}

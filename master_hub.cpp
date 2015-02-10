/*
 * master_hub.cpp
 *
 *  Created on: Feb 10, 2015
 *      Author: ace
 */

#include "master_hub.h"

namespace srnp
{

int MasterHub::buss = 0;
boost::random::mt19937 MasterHub::gen;

MasterHubSession::MasterHubSession (boost::asio::io_service& service) :
		socket_ (service)
{

}

MasterHubSession::~MasterHubSession ()
{

}

void MasterHubSession::handleRead(MasterHub* master, const boost::system::error_code& e)
{
	if(!e)
	{
		printf("\nGod! We received garbage data. Something is fishy...");
		boost::asio::async_read(this->socket_, boost::asio::buffer(this->in_buffer_), boost::bind(&MasterHubSession::handleRead, this, master, boost::asio::placeholders::error) );
	}
	else
	{
		master->sessions_map().erase(owner_);

		// Create a component info message and send to all with a delete message.
		UpdateComponents update_msg;
		update_msg.component.ip = socket_.remote_endpoint().address().to_string();
		//update_msg.component.owner = owner_;
		//update_msg.component.port = socket_.remote_endpoint().port();
		update_msg.operation = UpdateComponents::DELETE;

		master->sendUpdateComponentsMessageToAll(update_msg);

		printf("\nSession Collapse...");
		delete this;
	}

}

MasterHub::MasterHub(boost::asio::io_service& service, unsigned short port) :
		io_service_ (service),
		acceptor_ (service, tcp::endpoint(tcp::v4(), port)),
		heartbeat_timer_ (service, boost::posix_time::seconds(1))
{
	MasterHubSession* new_session = new MasterHubSession(io_service_);
	acceptor_.async_accept (new_session->socket(), boost::bind(&MasterHub::handleAcceptedConnection, this, new_session, boost::asio::placeholders::error));
	// Register a callback for the timer. Called ever second.
	heartbeat_timer_.async_wait (boost::bind(&MasterHub::onHeartbeat, this));
	printf("\nHere we are folks");
	startSpinThreads();
}

int MasterHub::makeNewOwnerId()
{
	//
	//boost::random::uniform_int_distribution<> dist(1, 1000);
	//return dist(gen);

	return (++MasterHub::buss);
}

void MasterHub::onHeartbeat()
{
	heartbeat_timer_.expires_at(heartbeat_timer_.expires_at() + boost::posix_time::seconds(1));
	heartbeat_timer_.async_wait (boost::bind(&MasterHub::onHeartbeat, this));
	printf("\n*********************************************************");
	printf("\nAcceptor State: %s", acceptor_.is_open() ? "Open" : "Closed");
	//printf("\nNo. of Active Sessions: %d", sessions_map_.size());
	printf("\n*********************************************************\n");
}

void MasterHub::startSpinThreads()
{
	for(int i = 0; i < 2; i++)
		spin_thread_[i] = boost::thread (boost::bind(&boost::asio::io_service::run, &io_service_));
	printf("\n[MASTER]: 2 separate listening threads have started.");
}

void MasterHub::handleAcceptedConnection (MasterHubSession* new_session, const boost::system::error_code& e)
{
	if(!e)
	{
		unsigned short port_of_server;

		boost::system::error_code errore;
		boost::asio::read (new_session->socket(), boost::asio::buffer (new_session->in_port()), errore);

		std::istringstream port_stream(std::string(new_session->in_port().elems, new_session->in_port().size()));
		port_stream >> std::hex >> port_of_server;

		printf("\nPORT RECEIVED: %d", port_of_server);
		// Get the port first.

		// Send this guy his owner_id. And all components we have.
		MasterMessage msg;
		msg.owner = makeNewOwnerId();


		printf("\nWe give owner ID: %d", msg.owner);

		new_session->setOwner (msg.owner);

		// Collect component info to send to this new guy.
		for(std::map <int, MasterHubSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
		{
			ComponentInfo info;
			info.ip = (iter->second)->socket().remote_endpoint().address().to_string();
			info.owner = (iter->first);
			info.port = port_of_server;

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

		boost::asio::async_read(new_session->socket(), boost::asio::buffer(new_session->in_buffer()), boost::bind(&MasterHubSession::handleRead, new_session, this, boost::asio::placeholders::error) );
		printf("\nFolks!");
		MasterHubSession* new_session_ = new MasterHubSession(io_service_);
		acceptor_.async_accept (new_session_->socket(), boost::bind(&MasterHub::handleAcceptedConnection, this, new_session_, boost::asio::placeholders::error));
	}
	else
	{
		printf("\nSession Collapses...");
		printf("\nSwaketch: %s", new_session->socket().is_open() ? "OPEN" : "CLOSED");
		printf("\nError: %s", e.message().c_str());
		delete new_session;
	}
}

void MasterHub::sendUpdateComponentsMessageToAll(UpdateComponents msg)
{
	for(std::map <int, MasterHubSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
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

		error = (iter->second)->sendSyncMsg(out_size_);
		printf("\nDone writing Size. Error: %s.", error.message().c_str());
		error = (iter->second)->sendSyncMsg(out_msg_);
		printf("\nDone writing Data. Error: %s.", error.message().c_str());

	}
}

void MasterHub::sendMasterMessageToComponent(MasterHubSession *new_session, MasterMessage msg)
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


MasterHub::~MasterHub()
{
	// TODO Auto-generated destructor stub
}

} /* namespace srnp */


int main()
{
	boost::asio::io_service io;

	srnp::MasterHub m (io, 11311);

	io.run();
}

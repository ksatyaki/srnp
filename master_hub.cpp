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
		SRNP_PRINT_WARNING << "God! We received garbage data. Something is fishy...";
		boost::asio::async_read(this->socket_, boost::asio::buffer(this->in_buffer_), master->strand().wrap(boost::bind(&MasterHubSession::handleRead, this, master, boost::asio::placeholders::error)));
	}
	else
	{
		master->sessions_map().erase(owner_);

		// Create a component info message and send to all with a delete message.
		UpdateComponents update_msg;
		//update_msg.component.ip = socket_.remote_endpoint().address().to_string();
		update_msg.component.owner = owner_;
		//update_msg.component.port = socket_.remote_endpoint().port();
		update_msg.operation = UpdateComponents::DELETE;

		master->sendUpdateComponentsMessageToAll(update_msg);

		SRNP_PRINT_INFO << "Component disconnected. IP: "<<update_msg.component.ip<<", OWNER: "<<update_msg.component.owner;
		delete this;
	}

}

MasterHub::MasterHub(boost::asio::io_service& service, unsigned short port) :
		io_service_ (service),
		acceptor_ (service, tcp::endpoint(tcp::v4(), port)),
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		strand_ (service)
{
	MasterHubSession* new_session = new MasterHubSession(io_service_);
	acceptor_.async_accept (new_session->socket(), strand_.wrap(boost::bind(&MasterHub::handleAcceptedConnection, this, new_session, boost::asio::placeholders::error)));
	// Register a callback for the timer. Called ever second.
	heartbeat_timer_.async_wait (boost::bind(&MasterHub::onHeartbeat, this));
	SRNP_PRINT_INFO << "Master started";
	startSpinThreads();
}

int MasterHub::makeNewOwnerId(std::string port_str)
{
	int port_no = std::atoi(port_str.c_str());
	gen.seed(port_no);
	//gen.seed(boost::posix_time::second_clock::local_time().time_of_day().seconds());
	boost::random::uniform_int_distribution<> dist(1, 1000);
		return dist(gen);
}

void MasterHub::onHeartbeat()
{
	heartbeat_timer_.expires_at(heartbeat_timer_.expires_at() + boost::posix_time::seconds(1));
	heartbeat_timer_.async_wait (boost::bind(&MasterHub::onHeartbeat, this));
	SRNP_PRINT_TRACE << "*********************************************************";
	SRNP_PRINT_TRACE << "Acceptor State: " << acceptor_.is_open() ? "Open" : "Closed";
	SRNP_PRINT_TRACE << "*********************************************************";
}

void MasterHub::startSpinThreads()
{
	for(int i = 0; i < 2; i++)
		spin_thread_[i] = boost::thread (boost::bind(&boost::asio::io_service::run, &io_service_));
	SRNP_PRINT_INFO << "Two separate listening threads have started.";
}

void MasterHub::handleAcceptedConnection (MasterHubSession* new_session, const boost::system::error_code& e)
{
	if(!e)
	{

		boost::system::error_code errore;
		boost::asio::read (new_session->socket(), boost::asio::buffer (new_session->in_port_size()), errore);

		size_t size_of_port;
		std::istringstream port_size_stream(std::string(new_session->in_port_size().elems, new_session->in_port_size().size()));
		port_size_stream >> std::hex >> size_of_port;

		new_session->in_data().resize(size_of_port);
		boost::asio::read (new_session->socket(), boost::asio::buffer (new_session->in_data()), errore);

		std::istringstream port_value (std::string(new_session->in_data().data(), new_session->in_data().size()));
		std::string port_of_server;
		port_value >> port_of_server;

		SRNP_PRINT_DEBUG << "PORT RECEIVED: " << port_of_server.c_str();
		// Get the port first.

		// Send this guy his owner_id. And all components we have.
		MasterMessage msg;
		msg.owner = makeNewOwnerId(port_of_server);

		new_session->setOwner (msg.owner);

		// Collect component info to send to this new guy.
		for(std::map <int, MasterHubSession*>::iterator iter = sessions_map_.begin(); iter != sessions_map_.end(); iter++)
		{
			ComponentInfo info;
			info.ip = (iter->second)->socket().remote_endpoint().address().to_string();
			info.owner = (iter->first);
			info.port = ports_map_[info.owner];

			msg.all_components.push_back(info);
		}

		// Send master message for this guy.
		sendMasterMessageToComponent(new_session, msg);

		// Create a component info message and send to all with a add message.
		UpdateComponents update_msg;
		update_msg.component.ip = new_session->socket().remote_endpoint().address().to_string();
		update_msg.component.owner = new_session->getOwner();
		update_msg.component.port = port_of_server;
		update_msg.operation = UpdateComponents::ADD;

		sendUpdateComponentsMessageToAll(update_msg);

		// Finally add this guy to the HashMap.
		sessions_map_[new_session->getOwner()] = new_session;
		ports_map_[new_session->getOwner()] = port_of_server;

		SRNP_PRINT_INFO << "New Connection received from port: "<< port_of_server <<". Assigned owner ID: " << msg.owner;

		boost::asio::async_read(new_session->socket(), boost::asio::buffer(new_session->in_buffer()), strand_.wrap(boost::bind(&MasterHubSession::handleRead, new_session, this, boost::asio::placeholders::error)));
		MasterHubSession* new_session_ = new MasterHubSession(io_service_);
		acceptor_.async_accept (new_session_->socket(), strand_.wrap(boost::bind(&MasterHub::handleAcceptedConnection, this, new_session_, boost::asio::placeholders::error)));
	}
	else
	{
		SRNP_PRINT_ERROR << "CONNECTION FAILED";
		SRNP_PRINT_ERROR << "Error: " << e.message().c_str();
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
			SRNP_PRINT_FATAL << "Couldn't set stream size.";
		}
		std::string out_size_ = size_stream.str();

		(iter->second)->sendAsyncMsg(out_size_);
		(iter->second)->sendAsyncMsg(out_msg_);
	}
}

void MasterHubSession::handleAsyncWriteMsg(const boost::system::error_code& ec)
{
	SRNP_PRINT_DEBUG << "[AsyncWrite handler]: Done writing to "<<owner_<<". Error: " << ec.message();
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
		SRNP_PRINT_FATAL << "Couldn't set stream size.";
	}
	std::string out_size_ = size_stream.str();

	new_session->sendAsyncMsg(out_size_);
	new_session->sendAsyncMsg(out_msg_);
}


MasterHub::~MasterHub()
{

}

} /* namespace srnp */

//void init()
//{
//	boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
//
//	//boost::log::add_console_log(std::cout , boost::log::keywords::format = "[%TimeStamp%]: %Message%");
//
//}

int main()
{
	boost::asio::io_service io;
	srnp::srnp_print_setup(boost::log::trivial::info);

	srnp::MasterHub m (io, 11311);

	io.run();
}

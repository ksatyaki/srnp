/*
 * server.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: ace
 */

#include "srnp/server.h"

namespace srnp
{

/** MASTERLINK CLASS **/
MasterLink::MasterLink(boost::asio::io_service& service, std::string master_ip, std::string master_port, boost::shared_ptr <ServerSession>& my_client_session, Server* server):
		socket_ (service),
		my_client_session_ (my_client_session),
		resolver_ (service),
		master_ip_(master_ip),
		master_port_(master_port)
{
	tcp::resolver::query query(master_ip, master_port);
	tcp::resolver::iterator endpoint_iterator_ = resolver_.resolve(query);

	try {
		boost::asio::connect(socket_, endpoint_iterator_);
	} catch (std::exception& ex) {
		SRNP_PRINT_FATAL << "Exception when trying to connect to master: " << ex.what();
		exit(0);
	}

	BOOST_LOG_TRIVIAL (info) << "Connected to master!\n";

	// Calc port:
	std::stringstream sss;
	sss << server->getPort();
	SRNP_PRINT_DEBUG << "Port computed is: " << sss.str();

	std::string port_final = sss.str().c_str();
	// SEND THE PORT WE ARE ON, FIRST. MOST IMPORTANT.
	std::ostringstream port_size_stream;
	port_size_stream << std::setw(sizeof(size_t)) << std::hex << port_final.size();
	std::string out_port_size = port_size_stream.str();

	boost::system::error_code error_co;
	boost::asio::write (socket_, boost::asio::buffer(out_port_size), error_co);
	SRNP_PRINT_DEBUG << "[MasterLink]: Writing port size to master_hub" << error_co.message();

	boost::asio::write (socket_, boost::asio::buffer(port_final), error_co);
	SRNP_PRINT_DEBUG << "[MasterLink]: Writing port to master_hub" << error_co.message();

	// READ STUFF.

	boost::asio::read (socket_, boost::asio::buffer(in_size_));

	size_t data_size;
	// Deserialize the length.
	std::istringstream size_stream(std::string(in_size_.elems, in_size_.size()));
	size_stream >> std::hex >> data_size;

	in_data_.resize(data_size);
	boost::asio::read(socket_, boost::asio::buffer(in_data_), error_co);

	// If we reach here, we are sure that we got a MasterMessage.

	std::istringstream mm_stream(std::string(in_data_.data(), in_data_.size()));
	boost::archive::text_iarchive header_archive(mm_stream);

	SRNP_PRINT_TRACE << "READ EVERTHING!";

	header_archive >> mm_;

	for(std::vector <ComponentInfo>::iterator iter = mm_.all_components.begin(); iter != mm_.all_components.end(); iter++)
	{
		SRNP_PRINT_DEBUG << "[SERVER]: Adding these information";
		SRNP_PRINT_DEBUG << "[SERVER]: PORT: " << iter->port;
		SRNP_PRINT_DEBUG << "[SERVER]: OWNER: " << iter->owner;
		SRNP_PRINT_DEBUG << "[SERVER]: IP: " << iter->ip;

		if(iter->ip.compare("127.0.0.1") == 0)
		{
			iter->ip = master_ip;
			SRNP_PRINT_DEBUG << "IP 127.0.0.1 should be changed to this: " << iter->ip;
		}
	}

	server->owner() = mm_.owner;
}

void MasterLink::sendMMToOurClientAndWaitForUCMsg()
{

	SRNP_PRINT_TRACE << "\n!!!!!We are waiting!!!!!";
	my_client_session_->sendMasterMsgToOurClient(mm_);

	// Start listening for update components messages.
	boost::asio::async_read(socket_, boost::asio::buffer(in_size_), boost::bind(&MasterLink::handleUpdateComponentsMsg, this, boost::asio::placeholders::error));

}

void MasterLink::handleUpdateComponentsMsg(const boost::system::error_code& e)
{
	size_t data_size;

	std::istringstream size_stream (std::string(in_size_.elems, in_size_.size()));
	size_stream >> std::hex >> data_size;

	in_data_.resize(data_size);
	boost::system::error_code errore;
	boost::asio::read(socket_, boost::asio::buffer(in_data_), errore);

	SRNP_PRINT_TRACE << "[Server]: Sync receive of UC message from MasterHub: " << errore.message();
	std::istringstream uc_stream(std::string(in_data_.data(), in_data_.size()));
	boost::archive::text_iarchive header_archive(uc_stream);

	UpdateComponents uc;
	header_archive >> uc;

	SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: IP: " << uc.component.ip;
	SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: OWNER: " << uc.component.owner;
	SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: PORT: " << uc.component.port;

	if(uc.component.ip.compare("127.0.0.1") == 0)
	{
		uc.component.ip = master_ip_;
		SRNP_PRINT_DEBUG << "I changed ip to this: " << uc.component.ip;
	}

	my_client_session_->sendUpdateComponentsMsgToOurClient(uc);
	boost::asio::async_read(socket_, boost::asio::buffer(in_size_), boost::bind(&MasterLink::handleUpdateComponentsMsg, this, boost::asio::placeholders::error));
}


/** SERVERSESSION CLASS **/

int ServerSession::session_counter = 0;

ServerSession::ServerSession (boost::asio::io_service& service, PairSpace& pair_space, std::queue <Pair>& pair_queue) :
		socket_ (service),
		pair_queue_ (pair_queue),
		pair_space_ (pair_space)
{

}

ServerSession::~ServerSession ()
{
	socket_.close();
}

void ServerSession::startReading()
{
	boost::asio::async_read(socket_, boost::asio::buffer(in_header_size_buffer_), boost::bind(&ServerSession::handleReadHeaderSize, this, boost::asio::placeholders::error) );
}

void ServerSession::handleReadHeaderSize (const boost::system::error_code& e)
{
	SRNP_PRINT_DEBUG << "[In Server::handleReadHeaderSize]: We got error: " << e.message();

	if(!e)
	{
		size_t header_size;
		// Deserialize the length.
		std::istringstream headersize_stream(std::string(in_header_size_buffer_.elems, sizeof(size_t)));
		headersize_stream >> std::hex >> header_size;
		//

		in_header_buffer_.resize (header_size);
		boost::asio::async_read(socket_, boost::asio::buffer(in_header_buffer_), boost::bind(&ServerSession::handleReadHeader, this, boost::asio::placeholders::error) );
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}
}

void ServerSession::handleReadHeader (const boost::system::error_code& e)
{
	SRNP_PRINT_DEBUG << "[Server::handleReadHeader]: We got error: " << e.message();

	if(!e)
	{
		size_t data_size;
		// Deserialize the length.
		std::istringstream header_stream(std::string(in_header_buffer_.data(), in_header_buffer_.size()));
		boost::archive::text_iarchive header_archive(header_stream);

		MessageHeader header;
		header_archive >> header;
		//

		if(header.type_ == MessageHeader::PAIR_NOCOPY)
		{
			Pair tuple = pair_queue_.front();
			pair_queue_.pop();
			pair_space_.addPair(tuple);

			// Added new pair!
			SRNP_PRINT_DEBUG << "[Server::handleReadHeader]: Added new pair!";
			std::pair<std::string, std::string> tuple_pair = tuple.getPair();
			SRNP_PRINT_DEBUG << "Pair: " << tuple_pair.first << ", " << tuple_pair.second;

			boost::asio::async_read(socket_, boost::asio::buffer(in_header_size_buffer_), boost::bind(&ServerSession::handleReadHeaderSize, this, boost::asio::placeholders::error) );
		}
		// PARSE ALL REQUESTS HERE!
		else if(header.type_ == MessageHeader::PAIR)
		{
			in_data_buffer_.resize (header.length_);
			boost::asio::async_read(socket_, boost::asio::buffer(in_data_buffer_), boost::bind(&ServerSession::handleReadData, this, boost::asio::placeholders::error) );
		}
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::handleReadData (const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_DEBUG << "[In Server::handleReadData]: We got error: " << e.message();

		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);

		Pair tuple;
		data_archive >> tuple;

		std::pair<std::string, std::string> tuple_pair = tuple.getPair();

		SRNP_PRINT_DEBUG << "We got a tuple: " << tuple_pair.first << ", " << tuple_pair.second;

		//std::string success ("you fucking passed!");
		//socket_.async_write_some(boost::asio::buffer(success), boost::bind(&ServerSession::handleWrite, this, boost::asio::placeholders::error));
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::handleWrite (const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_DEBUG << "[In Server::handleWrite]: Wrote data: " << e.message();
		boost::asio::async_read(socket_, boost::asio::buffer(in_header_buffer_), boost::bind(&ServerSession::handleReadHeader, this, boost::asio::placeholders::error) );
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}
}

boost::system::error_code ServerSession::sendMasterMsgToOurClient(MasterMessage msg)
{
	std::ostringstream msg_stream;
	boost::archive::text_oarchive msg_archive(msg_stream);
	msg_archive << msg;
	out_msg_ = msg_stream.str();
	// END

	MessageHeader header;
	header.length_ = out_msg_.size();
	header.type_ = MessageHeader::MM;
	std::ostringstream msg_header_stream;
	boost::archive::text_oarchive msg_header_archive (msg_header_stream);
	msg_header_archive << header;
	out_header_ = msg_header_stream.str();

	// Prepare header length
	std::ostringstream size_stream;
	size_stream << std::setw(sizeof(size_t)) << std::hex << out_header_.size();
	if (!size_stream || size_stream.str().size() != sizeof(size_t))
	{
		// Something went wrong, inform the caller.
		/*
					boost::system::error_code error(boost::asio::error::invalid_argument);
					socket_.io_service().post(boost::bind(handler, error));
					return;

		 */
	}
	out_size_ = size_stream.str();

	boost::system::error_code error;
	boost::asio::write(socket_, boost::asio::buffer(out_size_), error);
	SRNP_PRINT_DEBUG << "[MM SIZE]: Sent" << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header_), error);
	SRNP_PRINT_DEBUG << "[MM HEADER]: Sent" << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_msg_), error);
	SRNP_PRINT_DEBUG << "[MM MSG]: Sent" << error.message();
}

boost::system::error_code ServerSession::sendUpdateComponentsMsgToOurClient(UpdateComponents msg)
{
	std::ostringstream msg_stream;
	boost::archive::text_oarchive msg_archive(msg_stream);
	msg_archive << msg;
	out_msg_ = msg_stream.str();
	// END

	MessageHeader header;
	header.length_ = out_msg_.size();
	header.type_ = MessageHeader::UC;
	std::ostringstream msg_header_stream;
	boost::archive::text_oarchive msg_header_archive (msg_header_stream);
	msg_header_archive << header;
	out_header_ = msg_header_stream.str();

	// Prepare header length
	std::ostringstream size_stream;
	size_stream << std::setw(sizeof(size_t)) << std::hex << out_header_.size();
	if (!size_stream || size_stream.str().size() != sizeof(size_t))
	{
		SRNP_PRINT_FATAL << "Couldn't set stream size.";
	}
	out_size_ = size_stream.str();

	boost::system::error_code error;
	boost::asio::write(socket_, boost::asio::buffer(out_size_), error);
	SRNP_PRINT_DEBUG << "[UC SIZE]: Sent", error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header_), error);
	SRNP_PRINT_DEBUG << "[UC HEADER]: Sent", error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_msg_), error);
	SRNP_PRINT_DEBUG << "[UC MSG]: Sent", error.message();
}



Server::Server (boost::asio::io_service& service, std::string server_ip, std::string master_hub_ip, std::string master_hub_port, std::queue <Pair>& pair_queue) :
		acceptor_ (service, tcp::endpoint(boost::asio::ip::address::from_string(server_ip), 0)),
		strand_ (service),
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		io_service_ (service),
		owner_id_(-1),
		pair_queue_ (pair_queue),
		master_ip_ (master_hub_ip),
		master_port_ (master_hub_port)
{
	port_ = acceptor_.local_endpoint().port();
	if(!my_client_session_)
		my_client_session_ = boost::shared_ptr <ServerSession> (new ServerSession(service, pair_space_, pair_queue_));

	SRNP_PRINT_INFO << "Here we are folks. St. Alfonso's pancake breakfast!";
	// Register a callback for accepting new connections.
	acceptor_.async_accept (my_client_session_->socket(), boost::bind(&Server::handleAcceptedMyClientConnection, this, my_client_session_, boost::asio::placeholders::error));

	// Register a callback for the timer. Called ever second.
	heartbeat_timer_.async_wait (boost::bind(&Server::onHeartbeat, this));

	// Start the spin thread.
	startSpinThreads();
}

void Server::startSpinThreads()
{
	for(int i = 0; i < 4; i++)
		spin_thread_[i] = boost::thread (boost::bind(&boost::asio::io_service::run, &io_service_));
	SRNP_PRINT_DEBUG << "Four separate listening threads have started.";
}

void Server::handleAcceptedMyClientConnection (boost::shared_ptr <ServerSession>& client_session, const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_DEBUG << "[SERVER]: We connected to our own client. On " << client_session->socket().remote_endpoint().port();
		my_master_link_ = boost::shared_ptr <MasterLink> (new MasterLink(io_service_, master_ip_, master_port_, my_client_session_, this));
		my_master_link_->sendMMToOurClientAndWaitForUCMsg();
		client_session->startReading();
		ServerSession::session_counter++;
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_queue_);
		acceptor_.async_accept (new_session_->socket(), boost::bind(&Server::handleAcceptedConnection, this, new_session_, boost::asio::placeholders::error));
	}
	else
	{
		ServerSession::session_counter--;
		client_session.reset();
		SRNP_PRINT_FATAL << "We couldn't connect to our own client. Sucks!";
	}
}

void Server::handleAcceptedConnection (ServerSession* new_session, const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_DEBUG << "[SERVER]: We, connect to " << new_session->socket().remote_endpoint().address().to_string()
				<< ":" << new_session->socket().remote_endpoint().port();
		new_session->startReading();
		ServerSession::session_counter++;
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_queue_);
		acceptor_.async_accept (new_session_->socket(), boost::bind(&Server::handleAcceptedConnection, this, new_session_, boost::asio::placeholders::error));
	}
	else
	{
		ServerSession::session_counter--;
		delete new_session;
	}
}

void Server::onHeartbeat()
{
	// TODO: SEE IF EVERYTHING IS OK BEFORE DOING THIS!
	elapsed_time_ += boost::posix_time::seconds(1);
	heartbeat_timer_.expires_at(heartbeat_timer_.expires_at() + boost::posix_time::seconds(1));
	heartbeat_timer_.async_wait (boost::bind(&Server::onHeartbeat, this));
	SRNP_PRINT_TRACE << "*********************************************************";
	SRNP_PRINT_TRACE << "[SERVER] Elapsed time: " << elapsed_time_ << std::endl;
	SRNP_PRINT_TRACE << "[SERVER] Acceptor State: " << acceptor_.is_open() ? "Open" : "Closed";
	SRNP_PRINT_DEBUG << "[SERVER] No. of Active Sessions: ", ServerSession::session_counter;
	SRNP_PRINT_DEBUG << "*********************************************************";
}

void Server::waitForEver()
{
	SRNP_PRINT_DEBUG << "Starting to wait forever...";
	for(int i = 0; i < 4; i++)
	{
		spin_thread_[i].join();
	}
}

Server::~Server()
{

}

} /* namespace srnp */

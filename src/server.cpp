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
MasterLink::MasterLink(boost::asio::io_service& service, std::string master_ip, std::string master_port, boost::shared_ptr <ServerSession>& my_client_session, Server* server, int desired_owner_id):
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

	indicatePresence(server, desired_owner_id);

	// READ STUFF.

	boost::asio::read (socket_, boost::asio::buffer(in_size_));

	size_t data_size;
	// Deserialize the length.
	std::istringstream size_stream(std::string(in_size_.elems, in_size_.size()));
	size_stream >> std::hex >> data_size;

	boost::system::error_code error_co;
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
	SRNP_PRINT_DEBUG << "[SERVER]: Owner ID: "<< server->owner();
}

void MasterLink::indicatePresence(Server* server, int desired_owner_id)
{
	IndicatePresence indicatePresenceMsg;

	if(desired_owner_id != -1)
	{
		indicatePresenceMsg.force_owner_id = true;
		indicatePresenceMsg.owner_id = desired_owner_id;
	}

	// Calc port:
	std::stringstream sss;
	sss << server->getPort();
	SRNP_PRINT_DEBUG << "Port computed is: " << sss.str();

	indicatePresenceMsg.port = sss.str();

	// Serialize this message.
	std::ostringstream indicate_msg_stream;
	boost::archive::text_oarchive indicate_msg_archive (indicate_msg_stream);
	indicate_msg_archive << indicatePresenceMsg;
	std::string out_indicate_msg = indicate_msg_stream.str();

	// SEND THE PORT WE ARE ON, FIRST. MOST IMPORTANT.
	std::ostringstream size_stream;
	size_stream << std::setw(sizeof(size_t)) << std::hex << out_indicate_msg.size();
	std::string out_size = size_stream.str();

	boost::system::error_code error_co;
	boost::asio::write (socket_, boost::asio::buffer(out_size), error_co);
	SRNP_PRINT_DEBUG << "[MasterLink]: Writing port size to master_hub" << error_co.message();

	boost::asio::write (socket_, boost::asio::buffer(out_indicate_msg), error_co);
	SRNP_PRINT_DEBUG << "[MasterLink]: Writing port to master_hub" << error_co.message();
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
	if(!e)
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
	else
	{
		SRNP_PRINT_FATAL << "Master seems disconnected! God we're in deadly peril.";
		// Do something to attempt and reconnect.
	}
}


/** SERVERSESSION CLASS **/

int ServerSession::session_counter = 0;

ServerSession::ServerSession (boost::asio::io_service& service, PairSpace& pair_space, PairSpace& pair_space_subscribed,
		PairQueue& pair_queue, int& owner) :
		socket_ (service),
		pair_space_subscribed_ (pair_space_subscribed),
		pair_queue_ (pair_queue),
		pair_space_ (pair_space),
		owner_ (owner)
{


}

ServerSession::~ServerSession ()
{
	socket_.close();
}

void ServerSession::handleReadHeaderSize (const boost::system::error_code& e)
{
	SRNP_PRINT_TRACE << "[In Server::handleReadHeaderSize]: We got error: " << e.message();

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
	SRNP_PRINT_TRACE << "[Server::handleReadHeader]: We got error: " << e.message();

	if(!e)
	{
		size_t data_size;
		// Deserialize the length.
		std::istringstream header_stream(std::string(in_header_buffer_.data(), in_header_buffer_.size()));
		boost::archive::text_iarchive header_archive(header_stream);

		MessageHeader header;
		header_archive >> header;
		//

		if(header.type != MessageHeader::PAIR_NOCOPY)
			in_data_buffer_.resize (header.length);

		if(header.type == MessageHeader::PAIR_NOCOPY)
		{
			// CRITICAL SECTION!!!

			pair_queue_.pair_queue_mutex.lock();
			Pair tuple = pair_queue_.pair_queue.front();
			pair_queue_.pair_queue.pop();
			pair_queue_.pair_queue_mutex.unlock();

			// Redundant. But no loss.
			tuple.setOwner(owner_);

			pair_space_.addPair(tuple);

			std::vector <Pair>::iterator iter = pair_space_.getPairIteratorWithKey(tuple.getKey());
			sendPairUpdateToClient(iter);
			if(iter->callback_ != NULL)
				iter->callback_(*iter);

			// Added new pair!
			SRNP_PRINT_TRACE << "[Server::handleReadHeader]: Added new pair!";
			std::pair<std::string, std::string> tuple_pair = tuple.getPair();
			SRNP_PRINT_DEBUG << "Pair from our client: " << tuple_pair.first << ", " << tuple_pair.second << ", " << tuple.getOwner();

			// Send message to client.

			startReading();
		}

		else if(header.type == MessageHeader::PAIR)
		{
			boost::asio::async_read(socket_, boost::asio::buffer(in_data_buffer_), boost::bind(&ServerSession::handleReadPair, this, boost::asio::placeholders::error) );
		}
		else if(header.type == MessageHeader::PAIR_UPDATE)
		{
			boost::asio::async_read(socket_, boost::asio::buffer(in_data_buffer_), boost::bind(&ServerSession::handleReadPairUpdate, this, boost::asio::placeholders::error) );
		}
		else if(header.type == MessageHeader::SUBSCRIPTION)
		{
			boost::asio::async_read(socket_, boost::asio::buffer(in_data_buffer_), boost::bind(&ServerSession::handleReadSubscription, this, boost::asio::placeholders::error) );
		}
		else if(header.type == MessageHeader::CALLBACK)
		{
			boost::asio::async_read(socket_, boost::asio::buffer(in_data_buffer_), boost::bind(&ServerSession::handleReadCallback, this, boost::asio::placeholders::error) );
		}
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::sendPairUpdateToClient(std::vector <Pair>::iterator iter)
{
	if(iter->subscribers_.size() != 0)
	{
		std::string out_data_ = "";
		// END

		// Setup the message header.
		srnp::MessageHeader header (0, srnp::MessageHeader::PAIR_UPDATE);
		// Serialize the data first so we know how large it is.
		std::ostringstream header_archive_stream;
		boost::archive::text_oarchive header_archive(header_archive_stream);
		header_archive << header;
		std::string out_header_ = header_archive_stream.str();
		// END

		// Prepare header length
		std::ostringstream header_size_stream;
		header_size_stream << std::setw(sizeof(size_t))	<< std::hex << out_header_.size();
		if (!header_size_stream || header_size_stream.str().size() != sizeof(size_t))
		{
			SRNP_PRINT_DEBUG << "[sendPairUpdate]: Couldn't set stream size!";
		}
		std::string  out_header_size_ = header_size_stream.str();

		// CRITICAL SECTION!!!
		// Be sure that once we have pushed into the queue, we should also send the data.
		// Because if we don't, another thread could push to queue and send after we have
		// just pushed. And on the receiving end, our member will be popped.
		boost::mutex::scoped_lock scoped_mutex_lock(pair_queue_.pair_update_queue_mutex);
		pair_queue_.pair_update_queue.push(*iter);

		SRNP_PRINT_DEBUG << "Writing Data To Client.";

		boost::mutex::scoped_lock write_lock (socket_write_mutex);
		this->sendDataToClient (out_header_size_, out_header_, out_data_);
	}
}

bool ServerSession::sendDataToClient(const std::string& out_header_size, const std::string& out_header, const std::string& out_data)
{

	boost::system::error_code error;

	boost::asio::write(socket_, boost::asio::buffer(out_header_size), error);
	SRNP_PRINT_TRACE << "[sendPairUp]: Done writing header size. Error: " << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header), error);
	SRNP_PRINT_TRACE << "[sendPairUp]: Done writing header. Error: " << error.message();

	if(out_data.size() != 0)
	{
		boost::asio::write(socket_, boost::asio::buffer(out_data), error);
		SRNP_PRINT_TRACE << "[sendPairUp]: Done writing data. Error: " << error.message();
	}

	if(!error)
		return true;
	else return false;
}

void ServerSession::handleReadCallback(const boost::system::error_code& e)
{
	if(!e)
	{
		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);
		SubscriptionORCallback subscriptionORCallbackMsg;
		data_archive >> subscriptionORCallbackMsg;
		SRNP_PRINT_DEBUG << "We got a Callback Request: ";

		if(subscriptionORCallbackMsg.owner_id == this->owner_)
		{
			SRNP_PRINT_DEBUG << "Registering/cancelling a callback on our own sweet pair.";
			if(subscriptionORCallbackMsg.registering)
			{
				pair_queue_.callback_queue_mutex.lock();
				pair_space_.addCallback(this->owner_, subscriptionORCallbackMsg.key, pair_queue_.callback_queue.front());
				pair_queue_.callback_queue.pop();
				pair_queue_.callback_queue_mutex.unlock();
			}
			else
			{
				pair_space_.removeCallback(subscriptionORCallbackMsg.owner_id, subscriptionORCallbackMsg.key);
			}
		}

		else
		{
			SRNP_PRINT_DEBUG << "Registering/cancelling a callback on a subscribed pair.";
			if(subscriptionORCallbackMsg.registering)
			{
				pair_queue_.callback_queue_mutex.lock();
				pair_space_subscribed_.addCallback(subscriptionORCallbackMsg.owner_id, subscriptionORCallbackMsg.key, pair_queue_.callback_queue.front());
				pair_queue_.callback_queue.pop();
				pair_queue_.callback_queue_mutex.unlock();
			}
			else
			{
				pair_space_subscribed_.removeCallback(subscriptionORCallbackMsg.owner_id, subscriptionORCallbackMsg.key);
			}
		}

		startReading();
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::handleReadSubscription(const boost::system::error_code& e)
{
	if(!e)
	{
		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);
		SubscriptionORCallback subscriptionORCallbackMsg;
		data_archive >> subscriptionORCallbackMsg;
		SRNP_PRINT_DEBUG << "We got a subscription request: ";

		if(subscriptionORCallbackMsg.owner_id == this->owner_)
		{
			SRNP_PRINT_DEBUG << "Registering/cancelling a subscription on our own sweet pair.";
			if(subscriptionORCallbackMsg.registering)
			{
				pair_space_.addSubscription(this->owner_, subscriptionORCallbackMsg.key, subscriptionORCallbackMsg.subscriber);
			}
			else
			{
				pair_space_.removeSubscription(this->owner_, subscriptionORCallbackMsg.key, subscriptionORCallbackMsg.subscriber);
			}
		}

		else
		{
			SRNP_PRINT_FATAL << "Something is terribly wrong. Can't register a subscriber on a subscribed pair! I am doing nothing.";
		}

		startReading();
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::handleReadPairUpdate (const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_TRACE << "Got a pair update. Opening packet.";
		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);

		Pair tuple;
		data_archive >> tuple;
		SRNP_PRINT_DEBUG << "We got a PairUpdate: " << tuple;
		pair_space_subscribed_.addPair(tuple);

		std::vector <Pair>::iterator iter = pair_space_subscribed_.getPairIteratorWithOwnerAndKey(tuple.getOwner(), tuple.getKey());
		if(iter->callback_ != NULL)
		{
			SRNP_PRINT_DEBUG << "Making a callback!";
			iter->callback_(*iter);
		}
		else
		{
			SRNP_PRINT_DEBUG << "No callbacks to invoke.";
		}

		startReading();
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}

}

void ServerSession::handleReadPair (const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_TRACE << "[In Server::handleReadPair]: We got error: " << e.message();

		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);

		Pair tuple;
		data_archive >> tuple;

		std::pair<std::string, std::string> tuple_pair = tuple.getPair();

		SRNP_PRINT_DEBUG << "We got a Pair from another client: " << tuple_pair.first << ", " << tuple_pair.second;

		tuple.setOwner(owner_);
		pair_space_.addPair(tuple);

		std::vector <Pair>::iterator iter = pair_space_.getPairIteratorWithKey(tuple.getKey());
		sendPairUpdateToClient(iter);
		if(iter->callback_ != NULL)
			iter->callback_(*iter);

		startReading();
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
		SRNP_PRINT_TRACE << "[In Server::handleWrite]: Wrote data: " << e.message();
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
	header.length = out_msg_.size();
	header.type = MessageHeader::MM;
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

	return error;
}

boost::system::error_code ServerSession::sendUpdateComponentsMsgToOurClient(UpdateComponents msg)
{
	std::ostringstream msg_stream;
	boost::archive::text_oarchive msg_archive(msg_stream);
	msg_archive << msg;
	out_msg_ = msg_stream.str();
	// END

	MessageHeader header;
	header.length = out_msg_.size();
	header.type = MessageHeader::UC;
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

	return error;
}



Server::Server (boost::asio::io_service& service, std::string master_hub_ip, std::string master_hub_port, PairQueue& pair_queue, int desired_owner_id) :
		acceptor_ (service, tcp::endpoint(tcp::v4(), 0)),
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
		my_client_session_ = boost::shared_ptr <ServerSession> (new ServerSession(service, pair_space_, pair_space_subscribed_, pair_queue_, owner_id_));

	SRNP_PRINT_INFO << "Here we are folks. St. Alfonso's pancake breakfast!";
	// Register a callback for accepting new connections.
	acceptor_.async_accept (my_client_session_->socket(), boost::bind(&Server::handleAcceptedMyClientConnection, this, my_client_session_, desired_owner_id, boost::asio::placeholders::error));

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

void Server::handleAcceptedMyClientConnection (boost::shared_ptr <ServerSession>& client_session, int desired_owner_id, const boost::system::error_code& e)
{
	if(!e)
	{
		SRNP_PRINT_DEBUG << "[SERVER]: We connected to our own client. On " << client_session->socket().remote_endpoint().port();
		my_master_link_ = boost::shared_ptr <MasterLink> (new MasterLink(io_service_, master_ip_, master_port_, my_client_session_, this, desired_owner_id));
		my_master_link_->sendMMToOurClientAndWaitForUCMsg();
		client_session->startReading();
		ServerSession::session_counter++;
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_space_subscribed_, pair_queue_, owner_id_);
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
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_space_subscribed_, pair_queue_, owner_id_);
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
	SRNP_PRINT_DEBUG << "[SERVER] No. of Active Sessions: " << ServerSession::session_counter;
	SRNP_PRINT_TRACE << "*********************************************************";
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

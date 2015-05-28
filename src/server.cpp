/*
  server.cpp
  
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

	SRNP_PRINT_INFO << "Connected to master!\n";

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

	//SRNP_PRINT_TRACE << "READ EVERTHING!";

	header_archive >> mm_;

	for(std::vector <ComponentInfo>::iterator iter = mm_.all_components.begin(); iter != mm_.all_components.end(); iter++)
	{
		//SRNP_PRINT_DEBUG << "[SERVER]: Adding these information";
		//SRNP_PRINT_DEBUG << "[SERVER]: PORT: " << iter->port;
		//SRNP_PRINT_DEBUG << "[SERVER]: OWNER: " << iter->owner;
		//SRNP_PRINT_DEBUG << "[SERVER]: IP: " << iter->ip;

		if(iter->ip.compare("127.0.0.1") == 0)
		{
			iter->ip = master_ip;
			//SRNP_PRINT_DEBUG << "IP 127.0.0.1 should be changed to this: " << iter->ip;
		}
	}

	server->owner() = mm_.owner;
	//SRNP_PRINT_DEBUG << "[SERVER]: Owner ID: "<< server->owner();
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
	//SRNP_PRINT_DEBUG << "Port computed is: " << sss.str();

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
	//SRNP_PRINT_DEBUG << "[MasterLink]: Writing port size to master_hub" << error_co.message();

	boost::asio::write (socket_, boost::asio::buffer(out_indicate_msg), error_co);
	//SRNP_PRINT_DEBUG << "[MasterLink]: Writing port to master_hub" << error_co.message();
}

void MasterLink::sendMMToOurClientAndWaitForUCMsg()
{

	//SRNP_PRINT_TRACE << "\n!!!!!We are waiting!!!!!";
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

		//SRNP_PRINT_TRACE << "[Server]: Sync receive of UC message from MasterHub: " << errore.message();
		std::istringstream uc_stream(std::string(in_data_.data(), in_data_.size()));
		boost::archive::text_iarchive header_archive(uc_stream);

		UpdateComponents uc;
		header_archive >> uc;

		//SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: IP: " << uc.component.ip;
		//SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: OWNER: " << uc.component.owner;
		//SRNP_PRINT_DEBUG << "[UpdateComponentsMsg]: PORT: " << uc.component.port;

		if(uc.component.ip.compare("127.0.0.1") == 0)
		{
			uc.component.ip = master_ip_;
			//SRNP_PRINT_DEBUG << "I changed ip to this: " << uc.component.ip;
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

ServerSession::ServerSession (boost::asio::io_service& service, PairSpace& pair_space,
							  PairQueue& pair_queue, int& owner, Server* server) :
		socket_ (service),
		pair_queue_ (pair_queue),
		pair_space_ (pair_space),
		owner_ (owner),
		server_ (server)
{


}

ServerSession::~ServerSession ()
{
	socket_.close();
}

void ServerSession::handleReadHeaderSize (const boost::system::error_code& e)
{
	//SRNP_PRINT_TRACE << "[In Server::handleReadHeaderSize]: We got error: " << e.message();

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
	//SRNP_PRINT_TRACE << "[Server::handleReadHeader]: We got error: " << e.message();

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

			pair_space_.mutex.lock();
			pair_space_.addPair(tuple);
			Pair::ConstPtr pair_to_callback = Pair::ConstPtr(new Pair(*(pair_space_.getPairIteratorWithKey(tuple.getKey()))));
			pair_space_.mutex.unlock();

			if(pair_space_.u_callback_ != NULL) {
				pair_space_.u_callback_(pair_to_callback);
			}

			if(pair_to_callback->callbacks_.size() != 0)
			{
				if(owner_ != -1) {
					//SRNP_PRINT_DEBUG << "Making a simple callback";
					
					for(std::map<CallbackHandle, Pair::CallbackFunction>::const_iterator i = pair_to_callback->callbacks_.begin(); i != pair_to_callback->callbacks_.end(); i++) {
						i->second(pair_to_callback);	
					}
				}
			}
		
			sendPairUpdateToClient(*pair_to_callback);
			startReading();
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

void ServerSession::sendPairUpdateToClient(const Pair& to_up, int sub_only_one)
{
	if(to_up.subscribers_.size() != 0)
	{
		std::string out_data_ = "";
		// END

		// Setup the message header.
		srnp::MessageHeader header (0, srnp::MessageHeader::PAIR_UPDATE);

		if(sub_only_one != -1)
		{
			//SRNP_PRINT_DEBUG << "Setting header to special value...";
			header.type = MessageHeader::PAIR_UPDATE_2;
			header.subscriber__ = sub_only_one;
		}
		
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
			SRNP_PRINT_FATAL << "[sendPairUpdate]: Couldn't set stream size!";
		}
		std::string  out_header_size_ = header_size_stream.str();

		// CRITICAL SECTION!!!
		// Be sure that once we have pushed into the queue, we should also send the data.
		// Because if we don't, another thread could push to queue and send after we have
		// just pushed. And on the receiving end, our member will be popped.
		boost::mutex::scoped_lock scoped_mutex_lock(pair_queue_.pair_update_queue_mutex);
		pair_queue_.pair_update_queue.push(to_up);
		//SRNP_PRINT_DEBUG << "Queue push pui - pair update";

		//SRNP_PRINT_DEBUG << "Writing Data To Client.";

		boost::mutex::scoped_lock write_lock (socket_write_mutex);
		this->sendDataToClient (out_header_size_, out_header_, out_data_);
		//SRNP_PRINT_DEBUG << "SEND DATA pui - pair update";
	}
	//else
	//SRNP_PRINT_DEBUG << "No Subscribers...";
}

bool ServerSession::sendDataToClient(const std::string& out_header_size, const std::string& out_header, const std::string& out_data)
{

	boost::system::error_code error;

	boost::asio::write(socket_, boost::asio::buffer(out_header_size), error);
	//SRNP_PRINT_TRACE << "[sendPairUp]: Done writing header size. Error: " << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header), error);
	//SRNP_PRINT_TRACE << "[sendPairUp]: Done writing header. Error: " << error.message();

	if(out_data.size() != 0)
	{
		boost::asio::write(socket_, boost::asio::buffer(out_data), error);
		//SRNP_PRINT_TRACE << "[sendPairUp]: Done writing data. Error: " << error.message();
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

		// PAIR_SPACE_MUTEX_LOCK
		//pair_space_.mutexLock();
		boost::mutex::scoped_lock pair_space_lock(pair_space_.mutex);
		//SRNP_PRINT_DEBUG << "We got a Callback Request: ";

		//SRNP_PRINT_DEBUG << "Registering/cancelling a callback on a pair.";
		if(subscriptionORCallbackMsg.registering)
		{
			pair_queue_.callback_queue_mutex.lock();
			
			if(subscriptionORCallbackMsg.key.compare("*") == 0)
				pair_space_.addCallbackToAll(pair_queue_.callback_queue.front());
			else
				pair_space_.addCallback(subscriptionORCallbackMsg.key, pair_queue_.callback_queue.front());
			
			pair_queue_.callback_queue.pop();
			pair_queue_.callback_queue_mutex.unlock();
		}
		else
		{
			//pair_space_.removeCallback(subscriptionORCallbackMsg.key);
		}


		// PAIR_SPACE_MUTEX_UNLOCK
		//pair_space_.mutexUnlock();

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
		//SRNP_PRINT_DEBUG << "We got a subscription request: ";

		boost::mutex::scoped_lock pair_space_lock(pair_space_.mutex);
		
		//SRNP_PRINT_DEBUG << "Registering/cancelling a subscription on our own sweet pair.";
		if(subscriptionORCallbackMsg.registering)
		{
			if(subscriptionORCallbackMsg.key.compare("*") == 0)
			{
				//pair_space_.mutexLock();
				pair_space_.addSubscriptionToAll(subscriptionORCallbackMsg.subscriber);
				//pair_space_.mutexUnlock();
				
				const std::vector <Pair>& all_keys = pair_space_.getAllPairs();
				
				for(std::vector <Pair>::const_iterator iter = all_keys.begin(); iter != all_keys.end(); iter++)
				{
					//SRNP_PRINT_DEBUG << "Sending a pair to subscriber fellow...";
					if(iter->getOwner() == this->owner_)
					{
						//SRNP_PRINT_DEBUG << "Subscriber: " << subscriptionORCallbackMsg.subscriber;
						this->server_->my_client_session()->sendPairUpdateToClient(*iter, subscriptionORCallbackMsg.subscriber);
					}
				}
			}
			else
			{
				//pair_space_.mutexLock();
				pair_space_.addSubscription(subscriptionORCallbackMsg.key, subscriptionORCallbackMsg.subscriber);
				//pair_space_.mutexUnlock();

				const std::vector <Pair>::const_iterator pairIterator = pair_space_.getPairIteratorWithKey(subscriptionORCallbackMsg.key);
				if(pairIterator->getOwner() == this->owner_)
					this->server_->my_client_session()->sendPairUpdateToClient(*pairIterator, subscriptionORCallbackMsg.subscriber);
			}
				
		}
		else
		{
			//pair_space_.mutexLock();
			
			if(subscriptionORCallbackMsg.key.compare("*") == 0)
				pair_space_.removeSubscriptionToAll(subscriptionORCallbackMsg.subscriber);
			else
				pair_space_.removeSubscription(subscriptionORCallbackMsg.key, subscriptionORCallbackMsg.subscriber);

			//pair_space_.mutexUnlock();
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
		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		boost::archive::text_iarchive data_archive(data_stream);

		Pair tuple;
		data_archive >> tuple;
		//SRNP_PRINT_DEBUG << "We got a PairUpdate: " << tuple;

		pair_space_.mutex.lock();
		pair_space_.addPair(tuple);
		Pair::ConstPtr pair_to_callback = Pair::ConstPtr(new Pair(*(pair_space_.getPairIteratorWithKey(tuple.getKey()))));
		pair_space_.mutex.unlock();

		if(pair_space_.u_callback_ != NULL) {
			pair_space_.u_callback_(pair_to_callback);
		}

		if(pair_to_callback->callbacks_.size() != 0)
		{
			if(owner_ != -1) {
				//SRNP_PRINT_DEBUG << "Making a simple callback";
				for(std::map<CallbackHandle, Pair::CallbackFunction>::const_iterator i = pair_to_callback->callbacks_.begin(); i != pair_to_callback->callbacks_.end(); i++) {
					i->second(pair_to_callback);	
				}
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

void ServerSession::handleWrite (const boost::system::error_code& e)
{
	if(!e)
	{
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
	//SRNP_PRINT_DEBUG << "[MM SIZE]: Sent" << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header_), error);
	//SRNP_PRINT_DEBUG << "[MM HEADER]: Sent" << error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_msg_), error);
	//SRNP_PRINT_DEBUG << "[MM MSG]: Sent" << error.message();

	return error;
}

boost::system::error_code ServerSession::sendUpdateComponentsMsgToOurClient(UpdateComponents msg)
{
	// First Remove all subscriptions from this fellow.
	if(msg.operation == UpdateComponents::REMOVE)
	{
		//pair_space_.mutexLock();
		boost::mutex::scoped_lock pair_space_lock(pair_space_.mutex);
		pair_space_.removeSubscriptionToAll(msg.component.owner);
		//pair_space_.mutexUnlock();
	}
	
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
	//SRNP_PRINT_DEBUG << "[UC SIZE]: Sent", error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_header_), error);
	//SRNP_PRINT_DEBUG << "[UC HEADER]: Sent", error.message();

	boost::asio::write(socket_, boost::asio::buffer(out_msg_), error);
	//SRNP_PRINT_DEBUG << "[UC MSG]: Sent", error.message();

	return error;
}



Server::Server (boost::asio::io_service& service, std::string master_hub_ip, std::string master_hub_port, PairSpace& pair_space, PairQueue& pair_queue, int desired_owner_id) :
		acceptor_ (service, tcp::endpoint(tcp::v4(), 0)),
		strand_ (service),
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		io_service_ (service),
		owner_id_(-1),
		pair_space_ (pair_space),
		pair_queue_ (pair_queue),
		master_ip_ (master_hub_ip),
		master_port_ (master_hub_port)
{
	port_ = acceptor_.local_endpoint().port();
	if(!my_client_session_)
		my_client_session_ = boost::shared_ptr <ServerSession> (new ServerSession(service, pair_space_, pair_queue_, owner_id_, this));

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
	//SRNP_PRINT_DEBUG << "Four separate listening threads have started.";
}

void Server::handleAcceptedMyClientConnection (boost::shared_ptr <ServerSession>& client_session, int desired_owner_id, const boost::system::error_code& e)
{
	if(!e)
	{
		//SRNP_PRINT_DEBUG << "[SERVER]: We connected to our own client. On " << client_session->socket().remote_endpoint().port();
		my_master_link_ = boost::shared_ptr <MasterLink> (new MasterLink(io_service_, master_ip_, master_port_, my_client_session_, this, desired_owner_id));
		my_master_link_->sendMMToOurClientAndWaitForUCMsg();
		client_session->startReading();
		ServerSession::session_counter++;
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_queue_, owner_id_, this);
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
		//SRNP_PRINT_DEBUG << "[SERVER]: We, connect to " << new_session->socket().remote_endpoint().address().to_string()
				//<< ":" << new_session->socket().remote_endpoint().port();
		new_session->startReading();
		ServerSession::session_counter++;
		ServerSession* new_session_ = new ServerSession(io_service_, pair_space_, pair_queue_, owner_id_, this);
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
	//SRNP_PRINT_TRACE << "*********************************************************";
	//SRNP_PRINT_TRACE << "[SERVER] Elapsed time: " << elapsed_time_ << std::endl;
	//SRNP_PRINT_TRACE << "[SERVER] Acceptor State: " << acceptor_.is_open() ? "Open" : "Closed";
	//SRNP_PRINT_TRACE << "[SERVER] No. of Active Sessions: " << ServerSession::session_counter;
	//SRNP_PRINT_TRACE << "*********************************************************";
}

void Server::waitForEver()
{
	//SRNP_PRINT_DEBUG << "Starting to wait forever...";
	for(int i = 0; i < 4; i++)
	{
		spin_thread_[i].join();
	}
}

Server::~Server()
{
	//SRNP_PRINT_DEBUG << "SERVER CLOSES CLEANLY!";
}

} /* namespace srnp */

/*
 * client.cpp
 *
 *  Created on: Feb 8, 2015
 *      Author: ace
 */

#include <srnp/client.h>

namespace srnp {

void ClientSession::handleConnection(Client* client, const boost::system::error_code& err)
{
	if(!err)
	{
		reconnect_timer_.cancel();

		if(is_this_our_server_session_)
		{
			//SRNP_PRINT_DEBUG << "Connected to Our Own Server on: " << this->socket_->remote_endpoint().port();
			//SRNP_PRINT_TRACE << "We are on: " << this->socket_->local_endpoint().port();
			boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCandPairMsgs, this, client, boost::asio::placeholders::error));
		}
		else
		{
			//SRNP_PRINT_DEBUG << "Connected to a Server on: " << this->socket_->remote_endpoint().port();
			if(client == NULL)
			{
				SRNP_PRINT_ERROR << "NOLLE!";
			}
			sendSubscriptionMsgs(client);
		}

	}
	else
	{
		SRNP_PRINT_DEBUG << "Not connected to host. Will try again in 10 seconds.";
		reconnect_timer_.expires_from_now(boost::posix_time::seconds(RECONNECT_TIMEOUT));
		reconnect_timer_.async_wait(boost::bind(&ClientSession::reconnectTimerCallback, this, client));
	}

}

void ClientSession::sendSubscriptionMsgs(Client* client)
{
	for(std::vector<std::string>::iterator iter = client->subscribed_tuples_.begin(); iter!= client->subscribed_tuples_.end(); iter++)
	{
		SRNP_PRINT_DEBUG << "SENDING SUBSCRIPTION MSG: ";
		SubscriptionORCallback subs_msg;

		subs_msg.key = *iter;
		subs_msg.registering = true;
		subs_msg.owner_id = -1;
		subs_msg.subscriber = client->owner_id_;

		std::ostringstream data_stream;
		boost::archive::text_oarchive data_archive (data_stream);
		data_archive << subs_msg;
		std::string out_data_ = data_stream.str();
		// END

		// Setup the message header.
		srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::SUBSCRIPTION);
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
			SRNP_PRINT_FATAL << "[registerSubscription]: Couldn't set stream size.";
		}
		std::string  out_header_size_ = header_size_stream.str();

		boost::mutex::scoped_lock write_lock(server_write_mutex);
		sendDataToServer(out_header_size_, out_header_, out_data_);
	}

}

void ClientSession::handleMMandUCandPairMsgs(Client* client, const boost::system::error_code& error)
{
	if(!error)
	{
		size_t header_size;
		// Deserialize the length.
		std::istringstream size_stream(std::string(in_size_.elems, sizeof(size_t)));
		size_stream >> std::hex >> header_size;
		//
		in_header_.resize (header_size);

		boost::system::error_code sync_receive_error;
		boost::asio::read(*socket_, boost::asio::buffer(in_header_), sync_receive_error);
		//SRNP_PRINT_DEBUG << "[CLIENT]: Sync receive of Message header: " << sync_receive_error.message();

		std::istringstream header_in_stream (std::string(in_header_.data(), in_header_.size()));
		boost::archive::text_iarchive header_archive(header_in_stream);
		MessageHeader header;
		header_archive >> header;

		in_data_.resize(header.length);

		if(header.length != 0)
		{
			boost::asio::read(*socket_, boost::asio::buffer(in_data_), sync_receive_error);
			//SRNP_PRINT_DEBUG << "[CLIENT]: Sync receive of Message: " << sync_receive_error.message();
		}

		if(header.type == MessageHeader::MM)
		{
			std::istringstream data_in_stream (std::string(in_data_.data(), in_data_.size()));
			boost::archive::text_iarchive data_archive (data_in_stream);

			MasterMessage mm;
			data_archive >> mm;

			client->owner_id_ = mm.owner;
			for(std::vector <ComponentInfo>::iterator iter = mm.all_components.begin(); iter != mm.all_components.end(); iter++)
			{
				//SRNP_PRINT_DEBUG << "[CLIENT]: Adding these informations...";
				//SRNP_PRINT_DEBUG << "[CLIENT]: PORT: " << iter->port;
				//SRNP_PRINT_DEBUG << "[CLIENT]: OWNER: " << iter->owner;
				//SRNP_PRINT_DEBUG << "[CLIENT]: IP: %s" << iter->ip;

				// TODO SHARED RESOURCE??!?!?!
				client->sessions_map_[iter->owner] = new ClientSession(client->service_, iter->ip, iter->port, false, client);
			}

			//SRNP_PRINT_DEBUG << "[CLIENT]: Master message received!";
			SRNP_PRINT_INFO << "[CLIENT]: Owner ID: " << mm.owner;

		}
		else if(header.type == MessageHeader::UC)
		{
			std::istringstream data_in_stream (std::string(in_data_.data(), in_data_.size()));
			boost::archive::text_iarchive data_archive (data_in_stream);

			UpdateComponents uc;
			data_archive >> uc;

			if(uc.operation == UpdateComponents::ADD)
			{
				//SRNP_PRINT_DEBUG << "Adding session... ( " << uc.component.ip << uc.component.port << uc.component.owner << " )";
				client->sessions_map_[uc.component.owner] = new ClientSession(client->service_, uc.component.ip, uc.component.port, false, client);
			}

			else if(uc.operation == UpdateComponents::REMOVE)
			{
				//SRNP_PRINT_DEBUG << "Deleting session... (" << uc.component.ip << uc.component.port << uc.component.owner << " )";
				ClientSession* session_to_delete = client->sessions_map_[uc.component.owner];
				client->sessions_map_.erase(uc.component.owner);
				delete session_to_delete;
			}
		}
		else if (header.type == MessageHeader::PAIR_UPDATE)
		{
			client->pair_queue_.pair_update_queue_mutex.lock();
			Pair P = client->pair_queue_.pair_update_queue.front();
			client->pair_queue_.pair_update_queue.pop();
			client->pair_queue_.pair_update_queue_mutex.unlock();

			setPairUpdate(P, client);
		}
		else
		{
			SRNP_PRINT_FATAL << "We received a message that was neither UC nor MM nor PairUpdate. Weird!";
		}

		boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCandPairMsgs, this, client, boost::asio::placeholders::error));
	}
	else
	{
		SRNP_PRINT_FATAL << "Lost connection with our own server. Don't know what to do! Error: " << error.message();
	}

}

void ClientSession::reconnectTimerCallback(Client* client)
{
	SRNP_PRINT_DEBUG << "Reconnecting to host...";
	boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&ClientSession::handleConnection, this, client, boost::asio::placeholders::error));
}

ClientSession::ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port, bool is_this_our_server_session, Client* client) :
		reconnect_timer_ (service, boost::posix_time::seconds(RECONNECT_TIMEOUT)),
		resolver_(service),
		is_this_our_server_session_ (is_this_our_server_session),
		client_(client)
{
	socket_ = boost::shared_ptr<tcp::socket>(new tcp::socket (service));
	tcp::resolver::query query(host, port);
	endpoint_iterator_ = resolver_.resolve(query);
	boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&ClientSession::handleConnection, this, client_, boost::asio::placeholders::error));
}

bool ClientSession::sendDataToServer(const std::string& out_header_size, const std::string& out_header, const std::string& out_data)
{

	boost::system::error_code error;

	boost::asio::write(*socket_, boost::asio::buffer(out_header_size), error);
	//SRNP_PRINT_TRACE << "[sendPair]: Done writing header size. Error: " << error.message();

	boost::asio::write(*socket_, boost::asio::buffer(out_header), error);
	//SRNP_PRINT_TRACE << "[sendPair]: Done writing header. Error: " << error.message();

	if(out_data.size() != 0)
	{
		boost::asio::write(*socket_, boost::asio::buffer(out_data), error);
		//SRNP_PRINT_TRACE << "[sendPair]: Done writing data. Error: " << error.message();
	}

	if(!error)
		return true;
	else return false;
}

/******************************************************/
/*********************** CLIENT ***********************/
/******************************************************/

Client::Client(boost::asio::io_service& service, std::string our_server_ip, std::string our_server_port, PairQueue& pair_queue) :
		service_ (service),
		owner_id_ (-1),
		pair_queue_(pair_queue)
{
	my_server_session_ = boost::shared_ptr <ClientSession> (new ClientSession (service, our_server_ip, our_server_port, true, this));
}

bool Client::setPair(const std::string& key, const std::string& value)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (owner_id_, key, value);

	std::string out_data_ = "";
	// END

	// Setup the message header.
	srnp::MessageHeader header (0, srnp::MessageHeader::PAIR_NOCOPY);
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

	}
	std::string  out_header_size_ = header_size_stream.str();

	// CRITICAL SECTION!!!
	// Be sure that once we have pushed into the queue, we should also send the data.
	// Because if we don't, another thread could push to queue and send after we have
	// just pushed. And on the receiving end, our member will be popped.
	boost::mutex::scoped_lock scoped_mutex_lock(pair_queue_.pair_queue_mutex);
	pair_queue_.pair_queue.push(my_pair);

	//SRNP_PRINT_DEBUG << "Writing Data To Server.";
	boost::mutex::scoped_lock wirte_lock(socket_write_mutex);
	return my_server_session_->sendDataToServer(out_header_size_, out_header_, out_data_);

}

bool ClientSession::setPairUpdate(const Pair& pair, Client* client)
{
	for (std::vector <int>::const_iterator it = pair.subscribers_.begin(); it != pair.subscribers_.end(); it ++)
	{
		if(client->sessions_map_.find(*it) != client->sessions_map_.end())
		{
			std::ostringstream data_stream;
			boost::archive::text_oarchive data_archive (data_stream);
			data_archive << pair;
			std::string out_data_ = data_stream.str();
			// END

			// Setup the message header.
			srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::PAIR_UPDATE);
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
				SRNP_PRINT_FATAL << "[setPairUpdate]: Couldn't set stream size.";
			}
			std::string  out_header_size_ = header_size_stream.str();

			SRNP_PRINT_DEBUG << "Writing Pair Data To OTHER Server.";

			boost::mutex::scoped_lock write_lock(client->sessions_map_[*it]->server_write_mutex);
			client->sessions_map_[*it]->sendDataToServer(out_header_size_, out_header_, out_data_);
		}
		else
			SRNP_PRINT_DEBUG << "A subscriber cound't be found.";
	}

	return true;
}

bool Client::registerCallback(const std::string& key, Pair::CallbackFunction callback_fn)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	SubscriptionORCallback callback_msg;

	callback_msg.key = key;
	callback_msg.registering = true;
	callback_msg.owner_id = -1;

	std::ostringstream data_stream;
	boost::archive::text_oarchive data_archive (data_stream);
	data_archive << callback_msg;
	std::string out_data_ = data_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::CALLBACK);
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
		SRNP_PRINT_FATAL << "[registerCallback]: Couldn't set stream size.";
	}
	std::string  out_header_size_ = header_size_stream.str();


	// Scoped lock!
	boost::mutex::scoped_lock callback_queue_lock (pair_queue_.callback_queue_mutex);
	pair_queue_.callback_queue.push(callback_fn);
	boost::mutex::scoped_lock wirte_lock(socket_write_mutex);
	return my_server_session_->sendDataToServer(out_header_size_, out_header_, out_data_);
}

bool Client::cancelCallback(const std::string& key)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	SubscriptionORCallback callback_msg;

	callback_msg.key = key;
	callback_msg.registering = false;
	callback_msg.owner_id = -1;

	std::ostringstream data_stream;
	boost::archive::text_oarchive data_archive (data_stream);
	data_archive << callback_msg;
	std::string out_data_ = data_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::CALLBACK);
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
		SRNP_PRINT_FATAL << "[cancelCallback]: Couldn't set stream size.";
	}
	std::string  out_header_size_ = header_size_stream.str();

	boost::mutex::scoped_lock wirte_lock(socket_write_mutex);
	return my_server_session_->sendDataToServer(out_header_size_, out_header_, out_data_);
}

void Client::cancelSubscription(const std::string& key)
{
	std::vector <std::string>::iterator key_iter = std::find(subscribed_tuples_.begin(), subscribed_tuples_.end(), key);
	if(key_iter == subscribed_tuples_.end())
	{
		SRNP_PRINT_DEBUG << "NOT SUBSCRIBED";
		return;
	}

	subscribed_tuples_.erase(key_iter);
	// Serialize the tuple first.
	// So we set-up the header according to this.
	SubscriptionORCallback subs_msg;

	subs_msg.key = key;
	subs_msg.registering = false;
	subs_msg.owner_id = -1;
	subs_msg.subscriber = owner_id_;

	std::ostringstream data_stream;
	boost::archive::text_oarchive data_archive (data_stream);
	data_archive << subs_msg;
	std::string out_data_ = data_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::SUBSCRIPTION);
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
		SRNP_PRINT_FATAL << "[cancelSubscription]: Couldn't set stream size.";
	}
	std::string  out_header_size_ = header_size_stream.str();

	for(std::map <int, ClientSession*>::iterator iter = sessions_map_.begin(); iter!= sessions_map_.end(); iter++)
	{
		boost::mutex::scoped_lock write_lock((iter->second)->server_write_mutex);
		(iter->second)->sendDataToServer(out_header_size_, out_header_, out_data_);
	}
}

void Client::registerSubscription(const std::string& key)
{
	if(std::find(subscribed_tuples_.begin(), subscribed_tuples_.end(), key) != subscribed_tuples_.end())
	{
		SRNP_PRINT_DEBUG << "ALREADY SUBSCRIBED";
		return;
	}

	subscribed_tuples_.push_back(key);

	// Serialize the tuple first.
	// So we set-up the header according to this.
	SubscriptionORCallback subs_msg;

	subs_msg.key = key;
	subs_msg.registering = true;
	subs_msg.owner_id = -1;
	subs_msg.subscriber = owner_id_;

	std::ostringstream data_stream;
	boost::archive::text_oarchive data_archive (data_stream);
	data_archive << subs_msg;
	std::string out_data_ = data_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::SUBSCRIPTION);
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
		SRNP_PRINT_FATAL << "[registerSubscription]: Couldn't set stream size.";
	}
	std::string  out_header_size_ = header_size_stream.str();

	for(std::map <int, ClientSession*>::iterator iter = sessions_map_.begin(); iter!= sessions_map_.end(); iter++)
	{
		boost::mutex::scoped_lock write_lock((iter->second)->server_write_mutex);
		(iter->second)->sendDataToServer(out_header_size_, out_header_, out_data_);
	}
}


Client::~Client()
{

}

} /* namespace srnp */

/*
int main()
{
	std::pair <std::string, std::string> host_pair ("127.0.0.1", "33133");
	std::vector < std::pair <std::string, std::string> > vec_host_pairs;
	vec_host_pairs.push_back(host_pair);

	boost::asio::io_service service;

	std::queue <srnp::PairPtr> pair_queue;

	srnp::Client cli (service, vec_host_pairs, pair_queue);

	service.run();

	return 0;
}
*/

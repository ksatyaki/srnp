/*
  client.cpp - implementation of classes and functions in client.h
  
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
#include <srnp/client.h>

namespace srnp {

void ClientSession::handleConnection(Client* client, const boost::system::error_code& err)
{
	if(!err)
	{
		reconnect_timer_.cancel();

		if(is_this_our_server_session_)
		{
			SRNP_PRINT_DEBUG << "Connected to Our Own Server on: " << this->socket_->remote_endpoint().port();
			SRNP_PRINT_TRACE << "We are on: " << this->socket_->local_endpoint().port();
			boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCandPairMsgs, this, client, boost::asio::placeholders::error));
		}
		else
		{
			SRNP_PRINT_DEBUG << "Connected to a Server on: " << this->socket_->remote_endpoint().port();
			if(client == NULL)
			{
				SRNP_PRINT_ERROR << "NOLLE!";
			}
			sendSubscriptionMsgs(client);
		}

	}
	else
	{
		SRNP_PRINT_WARNING << "Not connected to host. Will try again in 10 seconds.";
		reconnect_timer_.expires_from_now(boost::posix_time::seconds(RECONNECT_TIMEOUT));
		reconnect_timer_.async_wait(boost::bind(&ClientSession::reconnectTimerCallback, this, client));
	}

}

ClientSession::~ClientSession() {
	boost::system::error_code ec;
	this->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	this->socket_.reset();
	//SRNP_PRINT_ERROR << "Closed a socket with error: " << ec.message();
}

void ClientSession::sendSubscriptionMsgs(Client* client)
{
	SRNP_PRINT_DEBUG << "SEND SUBSCRIPTION MSGS!" ;
	for(std::vector<std::string>::iterator iter = client->subscribed_tuples_.begin(); iter!= client->subscribed_tuples_.end(); iter++)
	{
		SRNP_PRINT_DEBUG << "SENDING SUBSCRIPTION MSG: ";
		SubscriptionORCallback subs_msg;

		subs_msg.key = *iter;
		subs_msg.registering = true;
		subs_msg.owner_id = this->endpoint_owner_id_;
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

	std::map <int, std::vector<std::string> >::iterator it = client->owner_id_to_subscribed_pairs_.find(this->endpoint_owner_id_);

	if(it == client->owner_id_to_subscribed_pairs_.end()) {
		SRNP_PRINT_DEBUG << "We aren't subscribed to any pairs from: " << this->endpoint_owner_id_;
	}
	else {
		if(it->second.empty()) {
			SRNP_PRINT_DEBUG << "We used to be... but now we aren't subscribed to any pairs from: ." << this->endpoint_owner_id_;
		}
		else {
			for(int i = 0; i < it->second.size(); i++) {
				SubscriptionORCallback subs_msg;

				SRNP_PRINT_DEBUG << "To new guy: " << it->second[i];
				subs_msg.key = it->second[i];
				subs_msg.registering = true;
				subs_msg.owner_id = it->first;
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

		//std::string output = header.type == MessageHeader::PAIR_UPDATE ? "PU" : "MM or UC";
		//SRNP_PRINT_DEBUG << "SRNP SERVER SAYS: %s" << output;

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
			this->endpoint_owner_id_ = mm.owner;
			client->owner_id_ = mm.owner;
			
			for(std::vector <ComponentInfo>::iterator iter = mm.all_components.begin(); iter != mm.all_components.end(); iter++)
			{
				//SRNP_PRINT_DEBUG << "[CLIENT]: Adding these informations...";
				//SRNP_PRINT_DEBUG << "[CLIENT]: PORT: " << iter->port;
				//SRNP_PRINT_DEBUG << "[MM_MESSAGE FROM CLIENT]: OWNER: " << iter->owner;
				//SRNP_PRINT_DEBUG << "[CLIENT]: IP: %s" << iter->ip;

				// TODO SHARED RESOURCE??!?!?!
				client->sessions_map_[iter->owner] = new ClientSession(client->service_, iter->ip, iter->port, false, client, iter->owner);
			}

			//SRNP_PRINT_DEBUG << "[CLIENT]: Master message received!";
			//SRNP_PRINT_INFO << "[CLIENT]: Owner ID: " << mm.owner;

		}
		else if(header.type == MessageHeader::UC)
		{
			std::istringstream data_in_stream (std::string(in_data_.data(), in_data_.size()));
			boost::archive::text_iarchive data_archive (data_in_stream);

			UpdateComponents uc;
			data_archive >> uc;

			if(uc.operation == UpdateComponents::ADD)
			{
				SRNP_PRINT_DEBUG << "Adding session... ( " << uc.component.ip << uc.component.port << uc.component.owner << " )";
				client->sessions_map_[uc.component.owner] = new ClientSession(client->service_, uc.component.ip, uc.component.port, false, client, uc.component.owner);
			}

			else if(uc.operation == UpdateComponents::REMOVE)
			{
				//SRNP_PRINT_DEBUG << "Deleting session... (" << uc.component.ip << uc.component.port << uc.component.owner << " )";
				ClientSession* session_to_delete = client->sessions_map_[uc.component.owner];
				client->sessions_map_.erase(uc.component.owner);
				delete session_to_delete;
			}
		}
		else if (header.type == MessageHeader::PAIR_UPDATE || header.type == MessageHeader::PAIR_UPDATE_2)
		{
			client->pair_queue_.pair_update_queue_mutex.lock();
			Pair P = client->pair_queue_.pair_update_queue.front();
			client->pair_queue_.pair_update_queue.pop();
			client->pair_queue_.pair_update_queue_mutex.unlock();

			int only_one = -1;
			if(header.type == MessageHeader::PAIR_UPDATE_2)
			{
				//SRNP_PRINT_DEBUG << "PAIR_UPDATE_2 MSG...";
				only_one = header.subscriber__;
			}
			//else
			//	SRNP_PRINT_DEBUG << "Normal PairUpdate Msg...";
			
			setPairUpdate(P, client, only_one);
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

ClientSession::ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port, bool is_this_our_server_session, Client* client, const int& endpoint_owner_id) :
		reconnect_timer_ (service, boost::posix_time::seconds(RECONNECT_TIMEOUT)),
		resolver_(service),
		is_this_our_server_session_ (is_this_our_server_session),
		client_(client),
		endpoint_owner_id_(endpoint_owner_id)
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

Client::Client(boost::asio::io_service& service, std::string our_server_ip, std::string our_server_port, PairSpace& pair_space, PairQueue& pair_queue) :
		service_ (service),
		owner_id_ (-10),
		pair_space_(pair_space),
		pair_queue_(pair_queue),
		subscription_handle_new_ (0)
{
	my_server_session_ = boost::shared_ptr <ClientSession> (new ClientSession (service, our_server_ip, our_server_port, true, this, owner_id_));
}

bool Client::setPair(const std::string& key, const std::string& value, const Pair::PairType& type)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (owner_id_, key, value, type);

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
	boost::mutex::scoped_lock wirte_lock(my_server_session_->server_write_mutex);
	return my_server_session_->sendDataToServer(out_header_size_, out_header_, out_data_);

}

bool Client::setRemotePair(const int& owner, const std::string& key, const std::string& value, const Pair::PairType& type) {

	if(sessions_map_.find(owner) == sessions_map_.end()) {
		return false;
	}
	
    // Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (owner, key, value, type);

	std::ostringstream data_archive_stream;
	boost::archive::text_oarchive data_archive(data_archive_stream);
	data_archive << my_pair;
	std::string out_data_ = data_archive_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), MessageHeader::PAIR);
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
	boost::mutex::scoped_lock wirte_lock(sessions_map_[owner]->server_write_mutex);
	return sessions_map_[owner]->sendDataToServer(out_header_size_, out_header_, out_data_);
}

bool Client::setMetaPair(const int& meta_owner, const std::string& meta_key, const int& owner, const std::string& key) {
	boost::shared_array <char> buffer = boost::shared_array<char>(new char[100]);
	sprintf(buffer.get(), "(META %d ", owner);
	std::string value_ = std::string(buffer.get()) + key;
	if(meta_owner == this->owner_id_) {
		return setPair(meta_key, value_, Pair::META);
	}
	else {
		return setRemotePair(meta_owner, meta_key, value_, Pair::META);
	}
}

bool Client::initMetaPair(const int& meta_owner, const std::string& meta_key) {
	if(meta_owner == this->owner_id_) {
		return setPair(meta_key, "(META -1 NULL)", Pair::META);
	}
	else {
		return setRemotePair(meta_owner, meta_key, "(META -1 NULL)", Pair::META);
	}
}

Pair::ConstPtr Client::getPair(const int& owner, const std::string& key) {
	boost::mutex::scoped_lock pair_space_lock(pair_space_.mutex);
	return Pair::ConstPtr(new Pair(*(pair_space_.getPairIteratorWithOwnerAndKey(owner, key))));
}

bool ClientSession::setPairUpdate(const Pair& pair, Client* client, int subscriber_only_one)
{
	//SRNP_PRINT_DEBUG << "In setPairUpdate Now... with " << pair.subscribers_.size() << " no of subsribers --- oo: "<< subscriber_only_one;
	
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

	//SRNP_PRINT_DEBUG << "Writing Pair Data To OTHER Server/servers.";

	if(subscriber_only_one != -1)
	{
		//SRNP_PRINT_DEBUG << "ONE ONLY MAN!!!!";
		if(client->sessions_map_.find(subscriber_only_one) != client->sessions_map_.end())
		{
			boost::mutex::scoped_lock write_lock(client->sessions_map_[subscriber_only_one]->server_write_mutex);
			client->sessions_map_[subscriber_only_one]->sendDataToServer(out_header_size_, out_header_, out_data_);
			SRNP_PRINT_DEBUG << " Only one data sentings.";
		}
		else
		{
			SRNP_PRINT_FATAL << "A subscriber cound't be found.";
		}

		return true;
	}

	// Obvious else...
	for (std::vector <int>::const_iterator it = pair.subscribers_.begin(); it != pair.subscribers_.end(); it ++)
	{
		if(client->sessions_map_.find(*it) != client->sessions_map_.end())
		{
			boost::mutex::scoped_lock write_lock(client->sessions_map_[*it]->server_write_mutex);
			client->sessions_map_[*it]->sendDataToServer(out_header_size_, out_header_, out_data_);
		}
		else
		{
			SRNP_PRINT_FATAL << "A subscriber cound't be found.";
			it--;
			usleep(100000);
			continue;
		}
			
	}

	return true;
}

CallbackHandle Client::registerCallback(const int& owner, const std::string& key, const Pair::CallbackFunction& callback_fn)
{
	boost::mutex::scoped_lock pair_space_lock (this->pair_space_.mutex);

	if(key.compare("*") == 0) {
		pair_space_.addCallbackToAll(callback_fn);
		return -1.0;
	}
	
	else {
		return pair_space_.addCallback(owner, key, callback_fn);
	}
}

void Client::cancelCallback(const double& cbid)
{
	boost::mutex::scoped_lock pair_space_lock(pair_space_.mutex);
	pair_space_.removeCallback(cbid);
}

void Client::cancelSubscription(const int& owner, const std::string& key) {

	std::vector <std::string>::iterator iter_to_del = std::find(owner_id_to_subscribed_pairs_[owner].begin(), owner_id_to_subscribed_pairs_[owner].end(), key);
	if(iter_to_del == owner_id_to_subscribed_pairs_[owner].end()) {
		SRNP_PRINT_WARNING << "Don't unsubscribe from messages that you aren't subscribed to!";
	}
	else {
		owner_id_to_subscribed_pairs_[owner].erase(iter_to_del);
	}
		
	if(sessions_map_.find(owner) == sessions_map_.end()) {
		//SRNP_PRINT_ERROR << "Not yet!";
		return;
	}
		
	SubscriptionORCallback subs_msg;

	subs_msg.key = key;
	subs_msg.registering = false;
	subs_msg.owner_id = owner;
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

	boost::mutex::scoped_lock write_lock(sessions_map_[owner]->server_write_mutex);
	sessions_map_[owner]->sendDataToServer(out_header_size_, out_header_, out_data_);
	
}

void Client::cancelSubscription(const std::string& key)
{
	std::vector <std::string>::iterator key_iter = std::find(subscribed_tuples_.begin(), subscribed_tuples_.end(), key);
	if(key_iter == subscribed_tuples_.end())
	{
		SRNP_PRINT_WARNING << "Unsubscribe attemped for a tuple that wasn't subscribed to in the first place.";
		return;
	}

	subscribed_tuples_.erase(key_iter);
	// Serialize the tuple first.
	// So we set-up the header according to this.
	for(std::map <int, ClientSession*>::iterator iter = sessions_map_.begin(); iter!= sessions_map_.end(); iter++)
	{
		SubscriptionORCallback subs_msg;

		subs_msg.key = key;
		subs_msg.registering = false;
		subs_msg.owner_id = iter->first;
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

		boost::mutex::scoped_lock write_lock((iter->second)->server_write_mutex);
		(iter->second)->sendDataToServer(out_header_size_, out_header_, out_data_);
	}
}

SubscriptionHandle Client::registerSubscription(const std::string& key)
{
	if(std::find(subscribed_tuples_.begin(), subscribed_tuples_.end(), key) != subscribed_tuples_.end())
	{
		SRNP_PRINT_WARNING << "Trying to re-subscribe to an already subscribed tuple.";
		return 0;
	}

	subscribed_tuples_.push_back(key);

	subscription_handle_to_key_multiple_[++subscription_handle_new_] = key;

	for(std::map <int, ClientSession*>::iterator iter = sessions_map_.begin(); iter!= sessions_map_.end(); iter++)
	{
		// Serialize the tuple first.
		// So we set-up the header according to this.
		SubscriptionORCallback subs_msg;

		subs_msg.key = key;
		subs_msg.registering = true;
		subs_msg.owner_id = iter->first;
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

	
		boost::mutex::scoped_lock write_lock((iter->second)->server_write_mutex);
		(iter->second)->sendDataToServer(out_header_size_, out_header_, out_data_);
	}

	return subscription_handle_new_;
}

SubscriptionHandle Client::registerSubscription(const int& owner, const std::string& key)
{
	if(std::find(subscribed_tuples_.begin(), subscribed_tuples_.end(), key) != subscribed_tuples_.end())
	{
		SRNP_PRINT_WARNING << "Trying to re-subscribe to an already subscribed tuple.";
		return 0;
	}

	std::vector <std::string>::iterator iter_to_check = std::find(owner_id_to_subscribed_pairs_[owner].begin(), owner_id_to_subscribed_pairs_[owner].end(), key);
	if(iter_to_check != owner_id_to_subscribed_pairs_[owner].end()) {
		SRNP_PRINT_WARNING << "Trying to re-subscribe to an already subscribed tuple.";
		return 0;
	}

	owner_id_to_subscribed_pairs_[owner].push_back(key);

	subscription_handle_to_owner_key_[++subscription_handle_new_] = std::pair<int, std::string> (owner, key);

	if(sessions_map_.find(owner) == sessions_map_.end()) {
		SRNP_PRINT_ERROR << "Not yet!";
		return subscription_handle_new_;
	}

	// Serialize the tuple first.
	// So we set-up the header according to this.
	SubscriptionORCallback subs_msg;

	subs_msg.key = key;
	subs_msg.registering = true;
	subs_msg.owner_id = owner;
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

	boost::mutex::scoped_lock write_lock(sessions_map_[owner]->server_write_mutex);
	sessions_map_[owner]->sendDataToServer(out_header_size_, out_header_, out_data_);

	return subscription_handle_new_;
}

void Client::cancelSubscription(const SubscriptionHandle& handle) {

	if(handle == 0) {
		SRNP_PRINT_WARNING << "You are trying to use a SubscriptionHandleError to unsubscribe. SubscriptionHandle of 0 means error!";
		return;
	}
	
	std::map <SubscriptionHandle, std::pair<int, std::string> >::iterator iter = subscription_handle_to_owner_key_.find(handle);
	if(iter == subscription_handle_to_owner_key_.end()) {
		std::map <SubscriptionHandle, std::string>::iterator iter_map2 = subscription_handle_to_key_multiple_.find(handle);
		if(iter_map2 == subscription_handle_to_key_multiple_.end()) {
			SRNP_PRINT_WARNING << "Trying to cancel a subscription that doesn't exist is a crime!";
			return;
		}
		else {
			cancelSubscription(iter_map2->second);
			subscription_handle_to_key_multiple_.erase(iter_map2);
		}
	}
	else {
		cancelSubscription(iter->second.first, iter->second.second);
		subscription_handle_to_owner_key_.erase(iter);
	}
}


Client::~Client()
{
	for(std::map <int, ClientSession*>::iterator it = sessions_map_.begin(); it != sessions_map_.end(); it++) {
		delete(it->second);
	}
	//SRNP_PRINT_INFO << "Client sessions closed.";

	sessions_map_.clear();
	this->my_server_session_.reset();

	//SRNP_PRINT_INFO << "CLIENT CLOSES CLEANLY!";
}

} /* namespace srnp */

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
			SRNP_PRINT_DEBUG << "Connected to Our Own Server on: " << this->socket_->remote_endpoint().port();
			SRNP_PRINT_TRACE << "We are on: " << this->socket_->local_endpoint().port();
			boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCMsgs, this, client, boost::asio::placeholders::error));
		}
		else
		{
			SRNP_PRINT_DEBUG << "Connected to a Server on: " << this->socket_->remote_endpoint().port();
		}

	}
	else
	{
		SRNP_PRINT_DEBUG << "Not connected to host. Will try again in 10 seconds.";
		reconnect_timer_.expires_from_now(boost::posix_time::seconds(RECONNECT_TIMEOUT));
		reconnect_timer_.async_wait(boost::bind(&ClientSession::reconnectTimerCallback, this, client));
	}

}

void ClientSession::handleMMandUCMsgs(Client* client, const boost::system::error_code& error)
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
		SRNP_PRINT_DEBUG << "[CLIENT]: Sync receive of Message header: " << sync_receive_error.message();

		std::istringstream header_in_stream (std::string(in_header_.data(), in_header_.size()));
		boost::archive::text_iarchive header_archive(header_in_stream);
		MessageHeader header;
		header_archive >> header;

		in_data_.resize(header.length);

		boost::asio::read(*socket_, boost::asio::buffer(in_data_), sync_receive_error);
		SRNP_PRINT_DEBUG << "[CLIENT]: Sync receive of Message: " << sync_receive_error.message();

		std::istringstream data_in_stream (std::string(in_data_.data(), in_data_.size()));
		boost::archive::text_iarchive data_archive (data_in_stream);

		if(header.type == MessageHeader::MM)
		{
			MasterMessage mm;
			data_archive >> mm;

			client->owner_id_ = mm.owner;
			for(std::vector <ComponentInfo>::iterator iter = mm.all_components.begin(); iter != mm.all_components.end(); iter++)
			{
				SRNP_PRINT_DEBUG << "[CLIENT]: Adding these informations...";
				SRNP_PRINT_DEBUG << "[CLIENT]: PORT: " << iter->port;
				SRNP_PRINT_DEBUG << "[CLIENT]: OWNER: " << iter->owner;
				SRNP_PRINT_DEBUG << "[CLIENT]: IP: %s" << iter->ip;

				// TODO SHARED RESOURCE??!?!?!
				client->sessions_map_[iter->owner] = new ClientSession(client->service_, iter->ip, iter->port);
			}

			SRNP_PRINT_DEBUG << "[CLIENT]: Master message received!";
			SRNP_PRINT_INFO << "[CLIENT]: Owner ID: " << mm.owner;

		}
		else if(header.type == MessageHeader::UC)
		{
			UpdateComponents uc;
			data_archive >> uc;

			if(uc.operation == UpdateComponents::ADD)
			{
				SRNP_PRINT_DEBUG << "Adding session... ( " << uc.component.ip << uc.component.port << uc.component.owner << " )";
				client->sessions_map_[uc.component.owner] = new ClientSession(client->service_, uc.component.ip, uc.component.port);
			}

			else if(uc.operation == UpdateComponents::REMOVE)
			{
				SRNP_PRINT_DEBUG << "Deleting session... (" << uc.component.ip << uc.component.port << uc.component.owner << " )";
				ClientSession* session_to_delete = client->sessions_map_[uc.component.owner];
				client->sessions_map_.erase(uc.component.owner);
				delete session_to_delete;
			}
		}
		else
		{
			SRNP_PRINT_FATAL << "We received a message that was neither UC nor MM. Weird!";
		}

		boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCMsgs, this, client, boost::asio::placeholders::error));
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
	SRNP_PRINT_TRACE << "[sendPair]: Done writing header size. Error: " << error.message();

	boost::asio::write(*socket_, boost::asio::buffer(out_header), error);
	SRNP_PRINT_TRACE << "[sendPair]: Done writing header. Error: " << error.message();

	if(out_data.size() != 0)
	{
		boost::asio::write(*socket_, boost::asio::buffer(out_data), error);
		SRNP_PRINT_TRACE << "[sendPair]: Done writing data. Error: " << error.message();
	}

	if(!error)
		return true;
	else return false;
}

/******************************************************/
/*********************** CLIENT ***********************/
/******************************************************/

Client::Client(boost::asio::io_service& service, std::string our_server_ip, std::string our_server_port, std::queue <Pair>& pair_queue) :
		service_ (service),
		owner_id_ (-1),
		pair_queue_(pair_queue)
{
	my_server_session_ = boost::shared_ptr <ClientSession> (new ClientSession (service, our_server_ip, our_server_port, true, this));

	/**
	// We create one Session per server.
	for(std::vector <std::pair <std::string, std::string> >::const_iterator iter = servers.begin() + 1; iter != servers.end(); iter++)
		client_sessions_.push_back(boost::shared_ptr <ClientSession> (new ClientSession(service, iter->first, iter->second) ));
		**/
}

bool Client::setPair(const std::string& key, const std::string& value)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (key, value, owner_id_);

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

	// TODO PAIR QUEUE SCOPED MUTEX HERE!
	pair_queue_.push(my_pair);

	SRNP_PRINT_DEBUG << "Writing Data To Server.";
	return my_server_session_->sendDataToServer(out_header_size_, out_header_, out_data_);

}

bool Client::setPair(const int& owner, const std::string& key, const std::string& value)
{
	if(sessions_map_.find(owner) == sessions_map_.end())
	{
		SRNP_PRINT_INFO << "Ha. gotcha! Quitting right away!";
		return false;
	}

	// Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (key, value, -1);

	std::ostringstream data_stream;
	boost::archive::text_oarchive data_archive (data_stream);
	data_archive << my_pair;
	std::string out_data_ = data_stream.str();
	// END

	// Setup the message header.
	srnp::MessageHeader header (out_data_.size(), srnp::MessageHeader::PAIR);
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
		SRNP_PRINT_ERROR << "[setPair (the other one)]: Couldn't set stream size.";
	}
	std::string  out_header_size_ = header_size_stream.str();

	// TODO PAIR QUEUE SCOPED MUTEX HERE!
	SRNP_PRINT_DEBUG << "Writing Pair Data To OTHER Server.";
	return sessions_map_[owner]->sendDataToServer(out_header_size_, out_header_, out_data_);

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

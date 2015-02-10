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
		printf("\nConnected to Our Own Server.");
		reconnect_timer_.cancel();

		if(is_this_our_server_session_)
			boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCMsgs, this, client, boost::asio::placeholders::error));
	}
	else
	{
		printf("\nNot connected to host. Will try again in 10 seconds.");
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
		printf("Sync receive of Message header: %s", sync_receive_error.message().c_str());

		std::istringstream header_in_stream (std::string(in_header_.data(), in_header_.size()));
		boost::archive::text_iarchive header_archive(header_in_stream);
		MessageHeader header;
		header_archive >> header;

		in_data_.resize(header.length_);

		boost::asio::read(*socket_, boost::asio::buffer(in_data_), sync_receive_error);
		printf("Sync receive of Message header: %s", sync_receive_error.message().c_str());

		std::istringstream data_in_stream (std::string(in_data_.data(), in_data_.size()));
		boost::archive::text_iarchive data_archive (data_in_stream);

		if(header.type_ == MessageHeader::MM)
		{
			MasterMessage mm;
			data_archive >> mm;

			// I get my owner id from here!
			client->owner_id_ = mm.owner;
			for(std::vector <ComponentInfo>::iterator iter = mm.all_components.begin(); iter != mm.all_components.end(); iter++)
			{
				std::stringstream s; s << iter->port;
				client->sessions_map_[iter->owner] = new ClientSession(client->service_, iter->ip, s.str());
			}

			printf("\nMaster message received!");

		}
		else if(header.type_ == MessageHeader::UC)
		{
			UpdateComponents uc;
			data_archive >> uc;

			if(uc.operation == UpdateComponents::ADD)
			{
				std::stringstream s; s << uc.component.port;
				printf("\nAdding session... (%s, %s, %d)!", uc.component.ip.c_str(), s.str().c_str(), uc.component.owner);
				client->sessions_map_[uc.component.owner] = new ClientSession(client->service_, uc.component.ip, s.str());
			}

			else if(uc.operation == UpdateComponents::DELETE)
			{
				printf("\nDeleting session... (%s, %d, %d)!", uc.component.ip.c_str(), uc.component.port, uc.component.owner);
				ClientSession* session_to_delete = client->sessions_map_[uc.component.owner];
				client->sessions_map_.erase(uc.component.owner);
				delete session_to_delete;
			}
		}
		else
		{
			printf("\nWe received a message that was neither UC nor MM. Weird!");
		}

		boost::asio::async_read(*socket_, boost::asio::buffer(in_size_), boost::bind(&ClientSession::handleMMandUCMsgs, this, client, boost::asio::placeholders::error));
	}
	else
	{
		printf("\nLost connection with our own server. Don't know what to do! %s", error.message().c_str());
	}

}

void ClientSession::reconnectTimerCallback(Client* client)
{
	printf("\nReconnecting to host...");
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

bool ClientSession::sendPair(const std::string& out_header_size, const std::string& out_header, const std::string& out_data)
{

	boost::system::error_code error;

	socket_->write_some(boost::asio::buffer(out_header_size), error);
	printf("\nDone writing header size. Error: %s.", error.message().c_str());

	socket_->write_some(boost::asio::buffer(out_header), error);
	printf("\nDone writing header. Error: %s.", error.message().c_str());

	if(out_data.size() != 0)
	{
		socket_->write_some(boost::asio::buffer(out_data), error);
		printf("\nDone writing data. Error: %s.", error.message().c_str());
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
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		pair_queue_(pair_queue)
{
	my_server_session_ = boost::shared_ptr <ClientSession> (new ClientSession (service, our_server_ip, our_server_port, true, this));

	/**
	// We create one Session per server.
	for(std::vector <std::pair <std::string, std::string> >::const_iterator iter = servers.begin() + 1; iter != servers.end(); iter++)
		client_sessions_.push_back(boost::shared_ptr <ClientSession> (new ClientSession(service, iter->first, iter->second) ));
		**/

	heartbeat_timer_.async_wait (boost::bind(&Client::onHeartbeat, this));
}

bool Client::setPair(const std::string& key, const std::string& value)
{
	// Serialize the tuple first.
	// So we set-up the header according to this.
	srnp::Pair my_pair (key, value, owner_id_);
	// TODO fix owner id.

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

	pair_queue_.push(my_pair);

	return my_server_session_->sendPair(out_header_size_, out_header_, out_data_);

}

void Client::onHeartbeat()
{
	// TODO: SEE IF EVERYTHING IS OK BEFORE DOING THIS!
	elapsed_time_ += boost::posix_time::seconds(1);
	heartbeat_timer_.expires_at(heartbeat_timer_.expires_at() + boost::posix_time::seconds(1));
	heartbeat_timer_.async_wait (boost::bind(&Client::onHeartbeat, this));
	printf("\n*********************************************************");
	printf("\n[CLIENT] Elapsed time: "); std::cout << elapsed_time_ << std::endl;
	printf("\n*********************************************************\n");
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

/*
 * client.cpp
 *
 *  Created on: Feb 8, 2015
 *      Author: ace
 */

#include <srnp/client.h>

namespace srnp {

void ClientSession::handleConnection(const boost::system::error_code& err)
{
	if(!err)
	{
		printf("\nConnected to host.");
		reconnect_timer_.cancel();
	}
	else
	{
		printf("\nNot connected to host. Will try again in 10 seconds.");
		reconnect_timer_.expires_from_now(boost::posix_time::seconds(RECONNECT_TIMEOUT));
		reconnect_timer_.async_wait(boost::bind(&ClientSession::reconnectTimerCallback, this));
	}

}

void ClientSession::reconnectTimerCallback()
{
	printf("\nReconnecting to host...");
	boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&ClientSession::handleConnection, this, boost::asio::placeholders::error));
}

ClientSession::ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port) :
		reconnect_timer_ (service, boost::posix_time::seconds(RECONNECT_TIMEOUT)),
		resolver_(service)
{
	socket_ = boost::shared_ptr<tcp::socket>(new tcp::socket (service));
	tcp::resolver::query query(host, port);
	endpoint_iterator_ = resolver_.resolve(query);
	boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&ClientSession::handleConnection, this, boost::asio::placeholders::error));
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

Client::Client(boost::asio::io_service& service, const int& owner_id, const std::vector< std::pair <std::string, std::string> >& servers, std::queue <Pair>& pair_queue) :
		service_ (service),
		owner_id_ (owner_id),
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		pair_queue_(pair_queue)
{
	my_server_session_ = boost::shared_ptr <ClientSession> (new ClientSession (service, "127.0.0.1", "33133"));

	// We create one Session per server.
	for(std::vector <std::pair <std::string, std::string> >::const_iterator iter = servers.begin(); iter != servers.end(); iter++)
		client_sessions_.push_back(boost::shared_ptr <ClientSession> (new ClientSession(service, iter->first, iter->second) ));

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
	srnp::MessageHeader header (0, srnp::PAIR_NOCOPY);
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
	printf("\nElapsed time: "); std::cout << elapsed_time_ << std::endl;
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

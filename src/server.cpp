/*
 * server.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: ace
 */

#include "srnp/server.h"

namespace srnp
{

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
	printf("\n[In Server::handleReadHeaderSize]: We got error: %s.", e.message().c_str());

	if(!e)
	{
		//printf("Reinterpretted size: %s\n", in_header_buffer_.elems);
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
	printf("\n[Server::handleReadHeader]: We got error: %s.", e.message().c_str());

	if(!e)
	{
		size_t data_size;
		// Deserialize the length.
		std::istringstream header_stream(std::string(in_header_buffer_.data(), in_header_buffer_.size()));
		boost::archive::text_iarchive header_archive(header_stream);

		MessageHeader header;
		header_archive >> header;
		//

		if(header.type_ == PAIR_NOCOPY)
		{
			Pair tuple = pair_queue_.front();
			pair_queue_.pop();
			pair_space_.addPair(tuple);

			// Added new pair!
			printf("\n[Server::handleReadHeader]: Added new pair!");
			std::pair<std::string, std::string> tuple_pair = tuple.getPair();
			printf("\nWe got a tuple: %s, %s\n", tuple_pair.first.c_str(), tuple_pair.second.c_str());

			boost::asio::async_read(socket_, boost::asio::buffer(in_header_size_buffer_), boost::bind(&ServerSession::handleReadHeaderSize, this, boost::asio::placeholders::error) );
		}
		// PARSE ALL REQUESTS HERE!
		else if(header.type_ == PAIR)
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
		printf("\n[In Server::handleReadData]: We got error: %s.", e.message().c_str());

		std::istringstream data_stream (std::string(in_data_buffer_.data(), in_data_buffer_.size()));
		//printf("Data that was read is: %s.", read_data.c_str());
		boost::archive::text_iarchive data_archive(data_stream);

		Pair tuple;
		data_archive >> tuple;

		std::pair<std::string, std::string> tuple_pair = tuple.getPair();

		printf("\nWe got a tuple: %s, %s\n", tuple_pair.first.c_str(), tuple_pair.second.c_str());

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
		printf("\n[In Server::handleWrite]: Wrote data: %s.\n", e.message().c_str());
		boost::asio::async_read(socket_, boost::asio::buffer(in_header_buffer_), boost::bind(&ServerSession::handleReadHeader, this, boost::asio::placeholders::error) );
	}
	else
	{
		ServerSession::session_counter--;
		delete this;
	}
}



Server::Server (boost::asio::io_service& service, const int& owner_id, const int& port, std::queue <Pair>& pair_queue) :
		acceptor_ (service, tcp::endpoint(tcp::v4(), port)),
		strand_ (service),
		heartbeat_timer_ (service, boost::posix_time::seconds(1)),
		io_service_ (service),
		owner_id_(owner_id),
		pair_queue_ (pair_queue)
{
	ServerSession* new_session = new ServerSession(service, pair_space_, pair_queue_);

	printf("\nHere we are folks. Alfonso's pancake breakfast!\n");
	// Register a callback for accepting new connections.
	acceptor_.async_accept (new_session->socket(), boost::bind(&Server::handleAcceptedConnection, this, new_session, boost::asio::placeholders::error));

	// Register a callback for the timer. Called ever second.
	heartbeat_timer_.async_wait (boost::bind(&Server::onHeartbeat, this));

	// Template from boost tutorial/documentation.
	// int const& (X::*get) () const = &X::get;

	printf("\nHere we are folks. Alfonso's pancake breakfasts!\n");
	// Start the spin thread.
	startSpinThreads();
}

void Server::startSpinThreads()
{
	for(int i = 0; i < 4; i++)
		spin_thread_[i] = boost::thread (boost::bind(&boost::asio::io_service::run, &io_service_));
	printf("\n4 separate listening threads have started.");
}

void Server::handleAcceptedConnection (ServerSession* new_session, const boost::system::error_code& e)
{
	if(!e)
	{
		printf("\n[SERVER]: We, %s, connect to %s, %d", new_session->socket().local_endpoint().address().to_string().c_str(), new_session->socket().remote_endpoint().address().to_string().c_str(), new_session->socket().remote_endpoint().port());
		printf("\n[In Server::handleAcceptedConnection]: We got error: %s.\n", e.message().c_str());
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
	printf("\n*********************************************************");
	printf("\n[SERVER] Elapsed time: "); std::cout << elapsed_time_ << std::endl;
	printf("\n[SERVER] Acceptor State: %s", acceptor_.is_open() ? "Open" : "Closed");
	printf("\n[SERVER] No. of Active Sessions: %d", ServerSession::session_counter);
	printf("\n*********************************************************\n");
}

void Server::waitForEver()
{
	printf("\nStarting to wait forever...\n");
	for(int i = 0; i < 4; i++)
	{
		spin_thread_[i].join();
	}
}

Server::~Server()
{

}

} /* namespace srnp */

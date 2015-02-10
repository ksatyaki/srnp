#include <test_client.h>

TestClient::TestClient(boost::asio::io_service& ioserv):
io_service_(ioserv),
resolver_(io_service_)
{

}

void TestClient::testServer(const std::string& host)
{
	socket_ = boost::shared_ptr<tcp::socket>(new tcp::socket (io_service_));
	tcp::resolver::query query(host, "33133");
	endpoint_iterator_ = resolver_.resolve(query);
	boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&TestClient::onConnection, this, boost::asio::placeholders::error));
}

void TestClient::onConnection(const boost::system::error_code& err)
{
	if(!err)
	{
		printf("\nNow connected!");

		// Serialize the tuple first.
		// So we set-up the header according to this.
		srnp::Pair my_pair ("key_chitt", "value_chitt", 195);
		std::ostringstream data_archive_stream;
		boost::archive::text_oarchive data_archive(data_archive_stream);
		data_archive << my_pair;
		out_data_ = data_archive_stream.str();
		// END

		// Setup the message header.
		srnp::MessageHeader header (data_archive_stream.str().size(), srnp::MessageHeader::PAIR);
		// Serialize the data first so we know how large it is.
		std::ostringstream header_archive_stream;
		boost::archive::text_oarchive header_archive(header_archive_stream);
		header_archive << header;
		out_header_ = header_archive_stream.str();
		// END

		// Prepare header length
		std::ostringstream header_size_stream;
		header_size_stream << std::setw(sizeof(size_t))	<< std::hex << out_header_.size();
		if (!header_size_stream || header_size_stream.str().size() != sizeof(size_t))
		{
			// Something went wrong, inform the caller.
			/*
		boost::system::error_code error(boost::asio::error::invalid_argument);
		socket_.io_service().post(boost::bind(handler, error));
		return;

			 */
		}
		out_header_size_ = header_size_stream.str();

		boost::system::error_code error;

		// Write the serialized data to the socket. We use "gather-write" to send
		// both the header and the data in a single write operation.
		socket_->write_some(boost::asio::buffer(out_header_size_), error);
		getchar();
		printf("\nDone writing header size. Error: %s.", error.message().c_str());

		socket_->write_some(boost::asio::buffer(out_header_), error);
		getchar();
		printf("\nDone writing header. Error: %s.", error.message().c_str());

		socket_->write_some(boost::asio::buffer(out_data_), error);
		getchar();
		printf("\nDone writing data. Error: %s.", error.message().c_str());
		////////////////////////////////////
		//std::vector<boost::asio::const_buffer> buffers;
		//buffers.push_back(boost::asio::buffer(out_header_size_));
		//buffers.push_back(boost::asio::buffer(out_header_));
		//buffers.push_back(boost::asio::buffer(out_data_));

		//boost::asio::async_write(socket_, buffers, handler);

		//socket_->write_some(buffers);
		////////////////////////////////////
		/*
		// Read reply.
		size_t len = socket_->read_some(boost::asio::buffer(buf), error);
		std::cout.write(buf.data(), len);

		if (error == boost::asio::error::eof)
			break; // Connection closed cleanly by peer.
		else if (error)
			throw boost::system::system_error(error); // Some other error.
		 */
	}
	else
	{
		printf("\nWe got error! %s", err.message().c_str());
		boost::asio::async_connect(*socket_, endpoint_iterator_, boost::bind(&TestClient::onConnection, this, boost::asio::placeholders::error));
	}

}

int main(int argn, char* argv[])
{
	if (argn < 2)
	{
		printf("\nUsage: test_client <host>\n");
		exit(0);
	}

	std::string host = argv[1];

	boost::asio::io_service ioservice;
	TestClient dt_s_c(ioservice);
	try {
		dt_s_c.testServer(host);
		ioservice.run();
	}
	catch (std::exception& ex)
	{
		printf("\nWe received an exception on the client side of things: %s.\n", ex.what());
	}

	exit(0);
}

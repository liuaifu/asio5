#include "session.h"
#include <boost/bind.hpp>
#include <boost/xpressive/xpressive.hpp>

void session::on_http2_read_request(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	char *buf = new char[read_client_buf.size()];
	memcpy(buf, boost::asio::buffer_cast<const char*>(read_client_buf.data()), read_client_buf.size());
	encrypt(buf, read_client_buf.size());
	std::string req(buf, read_client_buf.size());
	delete buf;
	read_client_buf.consume(req.size());
	boost::xpressive::sregex rx = boost::xpressive::sregex::compile("CONNECT ([^:]+):(\\d+) HTTP/1.[01].*", boost::xpressive::icase);
	boost::xpressive::smatch what;
	if (!boost::xpressive::regex_match(req, what, rx)) {
		stop();
		return;
	}

	std::string host = what.str(1);
	std::string port = what.str(2);

	try {
		//fixme 异步解析
		tcp::resolver resolver(io_service_);
		tcp::resolver::query query(tcp::v4(), host, port);
		tcp::resolver::iterator iterator = resolver.resolve(query);
		server_socket.async_connect(*iterator,
			strand_.wrap(
				boost::bind(
					&session::on_http2_connect_server, shared_from_this(),
					boost::asio::placeholders::error, iterator
				)
			)
		);
	}
	catch (const std::exception& e) {
		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << e.what();
		stop();
	}
}

void session::on_http2_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	if (error)
	{
		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		BOOST_LOG_TRIVIAL(error) << "[" << session_id << "] open " << endpoint_iterator->host_name() << " fail";

		stop();
		return;
	}

	BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] open " << endpoint_iterator->host_name();

	std::string ack = "HTTP/1.1 200 Connection Established\r\n\r\n";
	write_to_client(ack.c_str(), ack.size(), true);

	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(SOCKET_RECV_BUF_LEN);
	client_socket.async_read_some(buf,
		strand_.wrap(
			boost::bind(&session::on_read_client_data, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);

	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::on_read_server_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

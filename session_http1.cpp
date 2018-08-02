#include "session.h"
#include <boost/bind.hpp>
#include <boost/xpressive/xpressive.hpp>

void session::on_http1_read_request_head(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		stop();
		return;
	}

	http1_request_size = bytes_transferred;
	boost::asio::streambuf::const_buffers_type buf = read_client_buf.data();
	std::string req(boost::asio::buffers_begin(buf), boost::asio::buffers_begin(buf) + http1_request_size);

	boost::xpressive::sregex rx_length = boost::xpressive::sregex::compile(".*\r\nContent-Length:\\s+(\\d+)\r\n.*");
	boost::xpressive::smatch what;
	if (boost::xpressive::regex_match(req, what, rx_length)) {
		size_t size = boost::lexical_cast<size_t>(what.str(1));
		if (size > 0) {
			boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(size);
			boost::asio::async_read(client_socket, buf,
				strand_.wrap(
					boost::bind(
						&session::on_http1_read_request_body, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				)
			);
			return;
		}
	}

	//只有头部
	on_http1_read_request_body(error, 0);
}

void session::on_http1_read_request_body(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		stop();
		return;
	}

	if (bytes_transferred > 0) {
		read_client_buf.commit(bytes_transferred);
		http1_request_size = bytes_transferred;
	}

	boost::asio::streambuf::const_buffers_type buf = read_client_buf.data();
	std::string req(boost::asio::buffers_begin(buf), boost::asio::buffers_begin(buf) + http1_request_size);
	read_client_buf.consume(http1_request_size);
	http1_request_size = 0;

	boost::replace_first(req, "Proxy-Connection:", "Connection:");
	//去除url中的协议类型、主机名、端口，只保留路径和参数
	boost::xpressive::sregex rx_uri = boost::xpressive::sregex::compile("https*://[^/]*");
	req = boost::xpressive::regex_replace(req, rx_uri, "");

	if (client_recv_count > 0) {
		write_to_server(req.c_str(), req.size());
		client_recv_count++;

		//读取下一个请求
		boost::asio::async_read_until(
			client_socket, read_client_buf, "\r\n\r\n",
			strand_.wrap(
				boost::bind(
					&session::on_http1_read_request_head, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			)
		);
		return;
	}

	//读取主机名和端口
	boost::xpressive::sregex rx_host = boost::xpressive::sregex::compile(".*\r\nHost:\\s+(.*?)(:(\\d+))?\r\n.*");
	boost::xpressive::smatch what;
	if (!boost::xpressive::regex_match(req, what, rx_host)) {
		stop();
		return;
	}
	std::string host = what.str(1);
	std::string port = what.str(3);
	if (port == "")
		port = "80";

	try {
		//fixme 异步解析
		tcp::resolver resolver(io_service_);
		tcp::resolver::query query(tcp::v4(), host, port);
		tcp::resolver::iterator iterator = resolver.resolve(query);
		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << host << ":" << port;
		server_socket.async_connect(*iterator,
			strand_.wrap(
				boost::bind(
					&session::on_http1_connect_server, shared_from_this(), req,
					boost::asio::placeholders::error, iterator
				)
			)
		);
	}
	catch (const std::exception& e) {
		BOOST_LOG_TRIVIAL(error) << "[session" << session_id << "]" << e.what();
		stop();
	}

	client_recv_count++;

	//读取下一个请求
	boost::asio::async_read_until(
		client_socket, read_client_buf, "\r\n\r\n",
		strand_.wrap(
			boost::bind(
				&session::on_http1_read_request_head, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);
}

void session::on_http1_connect_server(const std::string req, const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		BOOST_LOG_TRIVIAL(error) << "[http1] open " << endpoint_iterator->host_name() << " fail";
		stop();
		return;
	}

	write_to_server(req.c_str(), req.size());

	//开始读取服务端待转发数据
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::on_http1_read_server_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::on_http1_read_server_data(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		stop();
		return;
	}

	write_to_client(server_buf, bytes_transferred);
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::on_http1_read_server_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}
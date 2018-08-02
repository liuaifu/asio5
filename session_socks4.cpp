#include "session.h"
#include <boost/bind.hpp>
//#include <boost/xpressive/xpressive.hpp>

void session::on_socks4_read_request_step1(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		stop();
		return;
	}

	read_client_buf.commit(bytes_transferred);

	//读取userid
	boost::asio::async_read_until(
		client_socket, this->read_client_buf, '\x00',
		strand_.wrap(
			boost::bind(
				&session::on_socks4_read_request_step2, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);
}

void session::on_socks4_read_request_step2(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		stop();
		return;
	}

	//这里不能commit，因为async_read_until自动commit了

	//现在socks4请求已经完整
	const char *req = (const char *)read_client_buf.data().data();
	size_t size = read_client_buf.size();
	if (req[1] != 1) {
		//不支持bind
		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << "not support bind";
		stop();
		return;
	}

	unsigned short port = ntohs(*(unsigned short *)(req + 2));
	unsigned int host = ::ntohl(*(unsigned int *)(req + 4));
	boost::asio::ip::address address = boost::asio::ip::address_v4::address_v4(host);
	std::string ip = address.to_string();

	read_client_buf.consume(size);

	try {
		tcp::resolver resolver(io_service_);
		tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<std::string>(port));
		tcp::resolver::iterator iterator = resolver.resolve(query);
		server_socket.async_connect(
			*iterator,
			strand_.wrap(
				boost::bind(
					&session::on_socks4_connect_server, shared_from_this(),
					boost::asio::placeholders::error, iterator
				)
			)
		);
	}
	catch (const std::exception& e) {
		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << e.what();
		stop();
		return;
	}
}

void session::on_socks4_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]" << error.message();
		BOOST_LOG_TRIVIAL(error) << "[session" << session_id << "]open " << endpoint_iterator->host_name() << " fail";
		stop();
		return;
	}

	BOOST_LOG_TRIVIAL(debug) << "[session" << session_id << "]open " << endpoint_iterator->host_name();

	const char ack[8] = { 0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	write_to_client(ack, sizeof(ack));

	//通道已建立，开始读取客户端待转发数据
	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(SOCKET_RECV_BUF_LEN);
	client_socket.async_read_some(buf,
		strand_.wrap(
			boost::bind(&session::on_read_client_data, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);

	//开始读取服务端待转发数据
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::on_read_server_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

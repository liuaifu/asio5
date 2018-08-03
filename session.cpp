#include "session.h"
#include <boost/bind.hpp>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/xpressive/xpressive.hpp>

session::session(boost::asio::io_service& _io_service, unsigned int session_id)
	:io_service_(_io_service), client_socket(_io_service), server_socket(_io_service), strand_(_io_service)
{
	client_recv_count = 0;
	http1_request_size = 0;
	proxy_type = PROXY_TYPE_UNKNOWN;
	this->session_id = session_id;
}

session::~session(void)
{
}

void session::start()
{
	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(3);
	boost::asio::async_read(client_socket, buf,
		strand_.wrap(
			boost::bind(
				&session::on_read_prefix, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);
}

void session::stop()
{
	try {
		server_socket.close();
	}
	catch (const std::exception&) {

	}

	try {
		client_socket.close();
	}
	catch (const std::exception&) {

	}
}

void session::encrypt(void *data, size_t size)
{
#ifndef ENCRYPT
	return;
#endif

	char *p = (char*)data;
	for (size_t i = 0; i < size; i++) {
		p[i] ^= 'F';
	}
}

void session::write_to_client(const char *data, size_t size, bool need_encrypt)
{
	boost::shared_ptr<char> data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	if(need_encrypt)
		encrypt(data_ptr.get(), size);

	boost::lock_guard<boost::mutex> lock(mutex_deque_to_client);

	bool write_in_progress = !deque_to_client.empty();
	deque_to_client.push_back(std::make_pair(size, data_ptr));
	if (!write_in_progress)
	{
		const std::pair<size_t, boost::shared_ptr<char> >& pkt = deque_to_client.front();
		boost::asio::async_write(client_socket,
			boost::asio::buffer(pkt.second.get(), pkt.first),
			strand_.wrap(boost::bind(&session::handle_client_write, shared_from_this(),
				pkt.second,
				boost::asio::placeholders::error)
			)
		);
	}
}

void session::write_to_server(const char *data, size_t size, bool need_decrypt)
{
	boost::shared_ptr<char> data_ptr = boost::shared_ptr<char>(new char[size]);
	memcpy(data_ptr.get(), data, size);
	if (need_decrypt)
		encrypt(data_ptr.get(), size);

	boost::lock_guard<boost::mutex> lock(mutex_deque_to_server);

	bool write_in_progress = !deque_to_server.empty();
	deque_to_server.push_back(std::make_pair(size, data_ptr));
	if (!write_in_progress)
	{
		const std::pair<size_t, boost::shared_ptr<char> >& pkt = deque_to_server.front();
		boost::asio::async_write(server_socket,
			boost::asio::buffer(pkt.second.get(), pkt.first),
			strand_.wrap(boost::bind(&session::handle_server_write, shared_from_this(),
				boost::asio::placeholders::error)
			)
		);
	}
}

void session::on_read_client_data(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;
		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	read_client_buf.commit(bytes_transferred);

	const char *data = (const char *)read_client_buf.data().data();
	size_t size = read_client_buf.size();

	write_to_server(data, size, true);
	read_client_buf.consume(size);

	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(SOCKET_RECV_BUF_LEN);
	client_socket.async_read_some(buf,
		strand_.wrap(boost::bind(&session::on_read_client_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::on_read_server_data(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;
		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从service"<<"读取到"<<bytes_transferred<<"字节";

	write_to_client(server_buf, bytes_transferred, true);
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::on_read_server_data, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::handle_client_write(boost::shared_ptr<char> data, const boost::system::error_code& error)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	boost::lock_guard<boost::mutex> lock(mutex_deque_to_client);
	deque_to_client.pop_front();
	if (!deque_to_client.empty())
	{
		const std::pair<size_t, boost::shared_ptr<char> >& pkt = deque_to_client.front();
		boost::asio::async_write(client_socket,
			boost::asio::buffer(pkt.second.get(), pkt.first),
			strand_.wrap(boost::bind(&session::handle_client_write, shared_from_this(),
				pkt.second,
				boost::asio::placeholders::error)
			)
		);
	}
}

void session::handle_server_write(const boost::system::error_code& error)
{
	if (error)
	{
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	boost::lock_guard<boost::mutex> lock(mutex_deque_to_server);
	deque_to_server.pop_front();

	if (!deque_to_server.empty()) {
		const std::pair<size_t, boost::shared_ptr<char> >& pkt = deque_to_server.front();
		boost::asio::async_write(server_socket,
			boost::asio::buffer(pkt.second.get(), pkt.first),
			strand_.wrap(boost::bind(&session::handle_server_write, shared_from_this(),
				boost::asio::placeholders::error)
			)
		);
	}
}

/**
* 收到客户端第一次发送数据的前3字节
* 在这里判断协议类型和版本
*/
void session::on_read_prefix(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	read_client_buf.commit(bytes_transferred);

	char data[3];
	memcpy(data, read_client_buf.data().data(), 3);
	encrypt(data, sizeof(data));
	if (memcmp(data, "OPT", 3) == 0 || //OPTIONS
		memcmp(data, "GET", 3) == 0 || //GET
		memcmp(data, "HEA", 3) == 0 || //HEAD
		memcmp(data, "POS", 3) == 0 || //POST
		memcmp(data, "DEL", 3) == 0 || //DELETE
		memcmp(data, "TRA", 3) == 0	//TRACE
		) {
		proxy_type = PROXY_TYPE_HTTP1;
		char suffix[] = "\r\n\r\n";
		encrypt(suffix, sizeof(suffix) - 1);
		boost::asio::async_read_until(
			client_socket, this->read_client_buf, suffix,
			strand_.wrap(
				boost::bind(
					&session::on_http1_read_request_head, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			)
		);
	}
	else if (memcmp(data, "CON", 3) == 0) {
		proxy_type = PROXY_TYPE_HTTP2;
		char suffix[] = "\r\n\r\n";
		encrypt(suffix, sizeof(suffix) - 1);
		boost::asio::async_read_until(client_socket, read_client_buf, suffix,
			strand_.wrap(
				boost::bind(
					&session::on_http2_read_request, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			)
		);
	}
	else if (data[0] == '\x04') {
		proxy_type = PROXY_TYPE_SOCKS4;
		boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(5);
		boost::asio::async_read(client_socket, buf,
			strand_.wrap(
				boost::bind(
					&session::on_socks4_read_request_step1, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			)
		);
	}
	else if (data[0] == '\x05') {
		proxy_type = PROXY_TYPE_SOCKS5;
		boost::system::error_code ec;
		on_socks5_read_request(ec, bytes_transferred);
	}
	else {
		stop();
	}
}

void session::on_socks5_read_request(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error) {
		if (error == boost::asio::error::operation_aborted)
			return;

		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		stop();
		return;
	}

	if (bytes_transferred > 0)
		read_client_buf.commit(bytes_transferred);
	size_t size = read_client_buf.size();
	boost::shared_ptr<char> ptr_buf = boost::shared_ptr<char>(new char[size]);
	memcpy(ptr_buf.get(), read_client_buf.data().data(), size);
	read_client_buf.consume(size);
	encrypt(ptr_buf.get(), size);
	const char *data = ptr_buf.get();

	//sockts5协议文档：https://tools.ietf.org/html/rfc1928
	client_recv_count++;
	if (client_recv_count == 1)
	{
		if (memcmp(data, "\x05\x01\x00", 3) != 0)
		{
			//service_ptr_->stop();
			stop();
			return;
		}
		/**
		METHOD的值有：
		X'00' 无验证需求
		X'01' 通用安全服务应用程序接口(GSSAPI)
		X'02' 用户名/密码(USERNAME/PASSWORD)
		X'03' 至 X'7F' IANA 分配(IANA ASSIGNED)
		X'80' 至 X'FE' 私人方法保留(RESERVED FOR PRIVATE METHODS)
		X'FF' 无可接受方法(NO ACCEPTABLE METHODS)
		*/
		write_to_client("\x05\x00", 2, true);
	}
	else if (client_recv_count == 2)
	{
		if (size < 10) {
			//service_ptr_->stop();
			stop();
			return;
		}

		//|VER|CMD|RSV|ATYP|DST.ADDR|DST.PORT|
		if (data[0] != 0x05 || data[1] != 0x01 || data[2] != 0x00 || (data[3] != 0x01 && data[3] != 0x03))
		{
			//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"client"<<"不支持的代理协议";
			//service_ptr_->stop();
			stop();
			return;
		}

		char host[128] = { 0 };
		char port[6] = { 0 };
		if (data[3] == 0x01) {
			sprintf(host, "%d.%d.%d.%d", (unsigned char)data[4], (unsigned char)data[5],
				(unsigned char)data[6], (unsigned char)data[7]);
			sprintf(port, "%d", ntohs(*(unsigned short*)(data + 8)));
		}
		else if (data[3] == 0x03) {
			size_t size = data[4];
			memcpy(host, data + 5, size);
			host[size] = 0;
			sprintf(port, "%d", ntohs(*(unsigned short*)(data + 5 + size)));
		}

		try {
			//fixme 异步解析
			tcp::resolver resolver(io_service_);
			tcp::resolver::query query(tcp::v4(), host, port);
			tcp::resolver::iterator iterator = resolver.resolve(query);
			server_socket.async_connect(*iterator,
				strand_.wrap(
					boost::bind(
						&session::on_socks5_connect_server, shared_from_this(),
						boost::asio::placeholders::error, iterator
					)
				)
			);
		}
		catch (const std::exception& e) {
			BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << e.what();
			stop();
		}
		return;
	}
	else {
		write_to_server(data, size, false);
	}

	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(SOCKET_RECV_BUF_LEN);
	client_socket.async_read_some(buf,
		strand_.wrap(boost::bind(&session::on_socks5_read_request, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::on_socks5_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	if (error)
	{
		BOOST_LOG_TRIVIAL(debug) << "[" << session_id << "] " << error.message();
		BOOST_LOG_TRIVIAL(error) << "[socks5] open " << endpoint_iterator->host_name() << "fail";
		stop();
		return;
	}
	else
	{
		server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
			strand_.wrap(boost::bind(&session::on_read_server_data, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
			)
		);
	}
	char ack[10] = { 0 };
	ack[0] = 0x05;
	ack[1] = 0x00;
	ack[2] = 0x00;
	ack[3] = 0x01;

	try {
		std::array<unsigned char, 4U> address = server_socket.local_endpoint().address().to_v4().to_bytes();
		unsigned short port = server_socket.local_endpoint().port();
		ack[4] = address[0];
		ack[5] = address[1];
		ack[6] = address[2];
		ack[7] = address[3];
		unsigned short *p_port = (unsigned short *)&ack[8];
		*p_port = htons(port);
	}
	catch (const std::exception&) {
		stop();
		return;
	}
	write_to_client(ack, sizeof(ack), true);

	boost::asio::streambuf::mutable_buffers_type buf = read_client_buf.prepare(SOCKET_RECV_BUF_LEN);
	client_socket.async_read_some(buf,
		strand_.wrap(
			boost::bind(&session::on_socks5_read_request, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		)
	);
}

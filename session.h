#pragma once

#include <map>
#include <deque>
#include <utility>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

using boost::asio::ip::tcp;
using namespace boost::log::trivial;

class session : public boost::enable_shared_from_this<session>
{
public:
	session(boost::asio::io_service& _io_service, unsigned int session_id);
	~session(void);

	void start();
	void stop();

	/**
	* 通用的方法
	*/
	void write_to_client(const char *data, size_t size);
	void write_to_server(const char *data, size_t size);
	void on_read_client_data(const boost::system::error_code& error, size_t bytes_transferred);
	void on_read_server_data(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_client_write(boost::shared_ptr<char> data, const boost::system::error_code& error);
	void handle_server_write(const boost::system::error_code& error);

	/**
	* 读取前3字节，以便判断代理类型
	*/
	void on_read_prefix(const boost::system::error_code& error, size_t bytes_transferred);
	
	/**
	* http普通代理
	*/
	void on_http1_read_request_head(const boost::system::error_code& error, size_t bytes_transferred);
	void on_http1_read_request_body(const boost::system::error_code& error, size_t bytes_transferred);
	void on_http1_connect_server(const std::string req, const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator);
	void on_http1_read_server_data(const boost::system::error_code& error, size_t bytes_transferred);

	/**
	* http隧道代理
	*/
	void on_http2_read_request(const boost::system::error_code& error, size_t bytes_transferred);
	void on_http2_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator);

	/**
	* socks4代理
	*/
	void on_socks4_read_request_step1(const boost::system::error_code& error, size_t bytes_transferred);
	void on_socks4_read_request_step2(const boost::system::error_code& error, size_t bytes_transferred);
	void on_socks4_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator);

	/**
	* socks5代理
	*/
	void on_socks5_read_request(const boost::system::error_code& error, size_t bytes_transferred);
	void on_socks5_connect_server(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator);

public:
	boost::mutex mutex_;
	boost::asio::io_service::strand strand_;
	tcp::socket client_socket;

private:
	enum { SOCKET_RECV_BUF_LEN = 1024 };
	enum PROXY_TYPE {
		PROXY_TYPE_UNKNOWN,
		PROXY_TYPE_HTTP1,
		PROXY_TYPE_HTTP2,
		PROXY_TYPE_SOCKS4,
		PROXY_TYPE_SOCKS5,
	};
	unsigned int session_id;
	boost::asio::io_service& io_service_;
	tcp::socket server_socket;
	PROXY_TYPE proxy_type;

	boost::asio::streambuf read_client_buf;
	char server_buf[SOCKET_RECV_BUF_LEN];

	std::deque<std::pair<size_t, boost::shared_ptr<char> > > deque_to_client;
	boost::mutex mutex_deque_to_client;

	std::deque<std::pair<size_t, boost::shared_ptr<char> > > deque_to_server;
	boost::mutex mutex_deque_to_server;

	int client_recv_count;
	size_t http1_request_size;
};
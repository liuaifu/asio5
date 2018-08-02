#pragma once

#include "session.h"

class service : public boost::enable_shared_from_this<service>
{
public:
	service(boost::asio::io_service& io_service, short port);
	~service();

	void start();
	void handle_accept(boost::asio::ip::tcp::socket &client_socket, const boost::system::error_code& error);

private:
	void async_accept();

	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::shared_ptr<session> session_ptr;
	unsigned int session_id;
};
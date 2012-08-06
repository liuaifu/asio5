#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>

using boost::asio::ip::tcp;
using namespace std;

class client;

class service: public boost::enable_shared_from_this<service>
{
public:
	service(boost::asio::io_service& io_service);
	~service(void);

	void setStop(boost::function<void(boost::shared_ptr<client>)> _stop);
	bool start();
	void stop();
	void write(char *data, size_t size);
	void setClient(boost::shared_ptr<client> ptr);

private:
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_write(const boost::system::error_code& error);

public:
	tcp::socket socket_;

private:
	enum { max_length = 1024 };
	char data_[max_length];
	boost::weak_ptr<client> client_ptr_;
	boost::function<void(boost::shared_ptr<client>)> stop_;
};

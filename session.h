#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "client.h"
#include "service.h"
#include "channel.hpp"
#include <map>

using namespace std;
using boost::asio::io_service;
using boost::asio::ip::tcp;
using boost::asio::deadline_timer;

typedef boost::weak_ptr<session> session_ptr;
typedef boost::shared_ptr<channel> channel_ptr;

class session : public boost::enable_shared_from_this<session>
{
public:
	session();
	~session(void);

	boost::shared_ptr<client> newSession();
	void start();
	void stop(boost::shared_ptr<client> client_ptr_);
	bool stopped() const;
	void check_deadline(deadline_timer* deadline);

public:
	boost::shared_ptr<client> client_ptr_;
	boost::shared_ptr<service> service_ptr_;
	channel_ptr channel_ptr_;
	std::map<client*,boost::shared_ptr<client>> map_;
};

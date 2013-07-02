#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/mutex.hpp>
#include "client.h"
#include <map>

using namespace std;

class session : public boost::enable_shared_from_this<session>
{
public:
	session();
	~session(void);

	boost::shared_ptr<client> newSession();
	void start();
	void stop(boost::shared_ptr<client> client_ptr_);

public:
	boost::shared_ptr<client> client_ptr_;
	std::map<client*,boost::shared_ptr<client> > sessions;
	boost::mutex mutex_;
};
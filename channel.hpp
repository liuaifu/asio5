#pragma once
#include <set>
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include "subscriber.hpp"

class session;

typedef boost::weak_ptr<session> session_ptr;

class channel
{
public:
	void join(session_ptr _session)
	{
		sessions_.insert(_session);
	}

	void leave(session_ptr _session)
	{
		sessions_.erase(_session);
	}

private:
	std::set<session_ptr> sessions_;
};


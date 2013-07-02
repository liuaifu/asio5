#include "session.h"
#include <boost/bind.hpp>
#include "service.h"

extern boost::asio::io_service g_io_service;
session::session()
{
}

session::~session(void)
{
}

boost::shared_ptr<client> session::newSession()
{
	static int id = 0;
	id++;
	client_ptr_ = boost::shared_ptr<client>(new client(g_io_service, id));
	boost::shared_ptr<service> service_ptr_ = boost::shared_ptr<service>(new service(g_io_service, id));
	client_ptr_->setStop(boost::bind(&session::stop, shared_from_this(), _1));
	service_ptr_->setStop(boost::bind(&session::stop, shared_from_this(), _1));
	client_ptr_->setService(service_ptr_);
	service_ptr_->setClient(client_ptr_);
	sessions[client_ptr_.get()] = client_ptr_;
	return client_ptr_;
}

void session::start()
{
	client_ptr_->start();
	cout << "session " << client_ptr_->getID() << " started" << endl;
}

void session::stop(boost::shared_ptr<client> client_ptr_)
{
	boost::mutex::scoped_lock lock(mutex_);
	std::map<client*, boost::shared_ptr<client> >::iterator it = sessions.find(client_ptr_.get());
	if (it != sessions.end()){
		it->second->socket().close();
		it->second->getService()->socket_.close();
		cout << "session " << it->second->getID() << " stopped" << endl;
		sessions.erase(it);
	}
	cout << "left " << sessions.size() << " session" << endl;
}

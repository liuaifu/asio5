#include "session.h"
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include "service.h"

extern boost::asio::io_service g_io_service;
session::session()
{
	cout<<__FUNCTION__<<endl;
}

session::~session(void)
{
	cout<<__FUNCTION__<<endl;
}

boost::shared_ptr<client> session::newSession()
{
	cout<<__FUNCTION__<<endl;
	static int id = 0;
	id++;
	client_ptr_ = boost::shared_ptr<client>(new client(g_io_service, id));
	boost::shared_ptr<service> service_ptr_ = boost::shared_ptr<service>(new service(g_io_service, id));
	client_ptr_->setStop(boost::bind(&session::stop, this, _1));
	service_ptr_->setStop(boost::bind(&session::stop, this, _1));
	client_ptr_->setService(service_ptr_);
	service_ptr_->setClient(client_ptr_);
	map_[client_ptr_.get()] = client_ptr_;
	return client_ptr_;
}

void session::start()
{
	cout<<__FUNCTION__<<endl;
	client_ptr_->start();
}

void session::stop(boost::shared_ptr<client> client_ptr_)
{
	BOOST_LOG_TRIVIAL(debug) << __FUNCTION__;
	cout<<__FUNCTION__<<endl;
	std::map<client*,boost::shared_ptr<client> >::iterator it = map_.find(client_ptr_.get());
	if(it!=map_.end()){
		//BOOST_LOG_TRIVIAL(debug) << it->second->id_ << ": 移除前引用" << it->second.use_count();
		it->second->socket().close();
		it->second->getService()->socket_.close();
		map_.erase(it);
	}
	BOOST_LOG_TRIVIAL(info)<<"剩余"<<map_.size()<<"个会话";
}

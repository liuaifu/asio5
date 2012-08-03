#include "session.h"

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
	client_ptr_ = boost::shared_ptr<client>(new client(g_io_service));
	service_ptr_ = boost::shared_ptr<service>(new service(g_io_service));
	client_ptr_->setStop(boost::bind(&session::stop, this, _1));
	service_ptr_->setStop(boost::bind(&session::stop, this, _1));
	map_[client_ptr_.get()] = client_ptr_;
	return client_ptr_;
}

void session::start()
{
	cout<<__FUNCTION__<<endl;
	client_ptr_->setService(service_ptr_);
	service_ptr_->setClient(client_ptr_);
	client_ptr_->start();
}

void session::stop(boost::shared_ptr<client> client_ptr_)
{
	cout<<__FUNCTION__<<endl;
	std::map<client*,boost::shared_ptr<client> >::iterator it = map_.find(client_ptr_.get());
	if(it!=map_.end()){
		map_.erase(it);
	}
	cout<<"Ê£Óà"<<map_.size()<<"¸ö»á»°"<<endl;
}

bool session::stopped() const
{
	cout<<__FUNCTION__<<endl;
	return !client_ptr_->socket_.is_open() || !service_ptr_->socket_.is_open();
}

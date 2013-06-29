#include "service.h"
#include <boost/bind.hpp>
//#include <boost/log/trivial.hpp>
#include "session.h"

service::service(boost::asio::io_service& io_service, int id)
	:socket_(io_service), id_(id)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
}

service::~service(void)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	socket_.close();
}

bool service::start()
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
			boost::bind(&service::handle_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	return true;
}

void service::setStop(boost::function<void(boost::shared_ptr<client>)> _stop)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	stop_ = _stop;
}

void service::stop()
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if(!client_ptr_.expired()){
		//BOOST_LOG_TRIVIAL(debug) << id_ << ": 引用" << client_ptr_.use_count();
		stop_(client_ptr_.lock());
	}//else
	//	BOOST_LOG_TRIVIAL(debug) << id_ << ": 已过期";
}

void service::setClient(boost::shared_ptr<client> ptr)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	client_ptr_ = ptr;
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": client_ptr use count=" << client_ptr_.use_count();
}

void service::write(char *data, size_t size)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"向service"<<"发送"<<size<<"字节";
	boost::asio::async_write(socket_,
		boost::asio::buffer(data, size),
		boost::bind(&service::handle_write, shared_from_this(),
		boost::asio::placeholders::error));
}

void service::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if(error){
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			this->stop();
			return;
		}
		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message();
		this->stop();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从service"<<"读取到"<<bytes_transferred<<"字节";

	if(!client_ptr_.expired()){
		client_ptr_.lock()->write(data_, bytes_transferred);
	}
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
		boost::bind(&service::handle_read, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

void service::handle_write(const boost::system::error_code& error)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			this->stop();
			return;
		}
		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"向服务器发送数据出错：错误码="<<error.value()<<", "<<error.message();
		this->stop();
		return;
	}
}

#include "service.h"
#include <boost/bind.hpp>
#include "session.h"

service::service(boost::asio::io_service& io_service)
	:socket_(io_service)
{
	cout<<__FUNCTION__<<endl;
}

service::~service(void)
{
	cout<<__FUNCTION__<<endl;
	socket_.close();
}

bool service::start(string host, string port)
{
	cout<<__FUNCTION__<<endl;
	tcp::resolver resolver(socket_.get_io_service());
    tcp::resolver::query socks_query(host, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(socks_query);
	try{
		boost::asio::connect(socket_, endpoint_iterator);
	}
	catch(std::exception ex)
	{
		cout<<ex.what()<<endl;
		this->stop();
		return false;
	}

	cout<<"service"<<"start"<<endl;
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
			boost::bind(&service::handle_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	return true;
}

void service::setStop(boost::function<void(boost::shared_ptr<client>)> _stop)
{
	stop_ = _stop;
}

void service::stop()
{
	cout<<__FUNCTION__<<endl;
	if(!client_ptr_.expired())
		stop_(client_ptr_.lock());
}

void service::setClient(boost::shared_ptr<client> ptr)
{
	cout<<__FUNCTION__<<endl;
	client_ptr_ = ptr;
}

bool service::connect(string host, string port)
{
	cout<<__FUNCTION__<<endl;
	tcp::resolver resolver(socket_.get_io_service());
	tcp::resolver::query query(tcp::v4(), host, port);
	tcp::resolver::iterator iterator = resolver.resolve(query);
	boost::system::error_code ec;
	boost::asio::connect(socket_, iterator, ec);
	if(ec){
		cout<<"service"<<"连接"<<host<<":"<<port<<"失败！"<<ec.message()<<endl;
		return false;
	}else
		return true;
}

void service::write(char *data, size_t size)
{
	cout<<__FUNCTION__<<endl;
	cout<<"向service"<<"发送"<<size<<"字节"<<endl;
	boost::asio::async_write(socket_,
		boost::asio::buffer(data, size),
		boost::bind(&service::handle_write, shared_from_this(),
		boost::asio::placeholders::error));
}

void service::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	cout<<__FUNCTION__<<endl;
	if(error){
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			this->stop();
			return;
		}
		cout<<"service"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message()<<endl;
		this->stop();
		return;
	}

	cout<<"从service"<<"读取到"<<bytes_transferred<<"字节"<<endl;

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
	cout<<__FUNCTION__<<endl;
	if (error)
	{
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			this->stop();
			return;
		}
		cout<<"service"<<"向服务器发送数据出错：错误码="<<error.value()<<", "<<error.message()<<endl;
		this->stop();
		return;
	}
}

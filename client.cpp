#include "client.h"
#include <iostream>
#include <boost/bind.hpp>
#include "session.h"

using namespace std;

client::client(boost::asio::io_service& io_service)
	:socket_(io_service)
{
	cout<<__FUNCTION__<<endl;
	recv_count = 0;
}

client::~client(void)
{
	cout<<__FUNCTION__<<endl;
	socket_.close();
}

void client::setStop(boost::function<void(boost::shared_ptr<client>)> _stop)
{
	stop_ = _stop;
}

void client::start()
{
	cout<<__FUNCTION__<<endl;
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
		boost::bind(&client::handle_read, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

void client::stop()
{
	cout<<__FUNCTION__<<endl;
	stop_(shared_from_this());
}

void client::write(char *data, size_t size)
{
	cout<<__FUNCTION__<<endl;
	cout<<"向client"<<"发送"<<size<<"字节"<<endl;
	boost::asio::async_write(socket_,
		boost::asio::buffer(data, size),
		boost::bind(&client::handle_write, shared_from_this(),
		boost::asio::placeholders::error));
}

void client::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	cout<<__FUNCTION__<<endl;
	if(error){
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			this->stop();
			return;
		}
		cout<<"client"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message()<<endl;
		//service_ptr_->stop();
		this->stop();
		return;
	}

	cout<<"从client"<<"读取到"<<bytes_transferred<<"字节"<<endl;
	recv_count++;
	if(recv_count==1)
	{
		if(bytes_transferred<3){
			//service_ptr_->stop();
			this->stop();
			return;
		}

		if(data_[0]!=0x05/* || data_[1]!=0x01 || data_[2]!=0x00 || data_[3]!=0x01*/)
		{
			//service_ptr_->stop();
			this->stop();
			return;
		}
		/*
		METHOD的值有：
		X'00' 无验证需求
		X'01' 通用安全服务应用程序接口(GSSAPI)
		X'02' 用户名/密码(USERNAME/PASSWORD)
		X'03' 至 X'7F' IANA 分配(IANA ASSIGNED)
		X'80' 至 X'FE' 私人方法保留(RESERVED FOR PRIVATE METHODS)
		X'FF' 无可接受方法(NO ACCEPTABLE METHODS)
		*/
		data_[0] = '\x05';
		data_[1] = '\x00';
		boost::asio::async_write(socket_,
			boost::asio::buffer(data_, 2),
			boost::bind(&client::handle_write, shared_from_this(),
			boost::asio::placeholders::error));
	}
	else if(recv_count==2)
	{
		if(bytes_transferred<10){
			//service_ptr_->stop();
			this->stop();
			return;
		}

		//|VER|CMD|RSV|ATYP|DST.ADDR|DST.PORT|
		if(data_[0]!=0x05 || data_[1]!=0x01 || data_[2]!=0x00 || (data_[3]!=0x01 && data_[3]!=0x03))
		{
			cout<<"client"<<"不支持的代理协议"<<endl;
			//service_ptr_->stop();
			this->stop();
			return;
		}

		char host[128];
		char port[6];
		if(data_[3]==0x01) {
			sprintf(host, "%d.%d.%d.%d", (unsigned char)data_[4], (unsigned char)data_[5], 
				(unsigned char)data_[6], (unsigned char)data_[7]);
			sprintf(port, "%d", ntohs(*(unsigned short*)(data_+8)));
		}else if(data_[3]==0x03) {
			memcpy(host, data_ + 5, data_[4]);
			host[data_[4]] = 0;
			sprintf(port, "%d", ntohs(*(unsigned short*)(data_ + 5 + data_[4])));
		}

		if(service_ptr_->start(host, port)){
			data_[1] = 0x00;
		}else{
			data_[1] = 0x01;
		}
		boost::asio::async_write(socket_,
			boost::asio::buffer(data_, bytes_transferred),
			boost::bind(&client::handle_write, shared_from_this(),
			boost::asio::placeholders::error));
	}else{
		service_ptr_->write(data_, bytes_transferred);
	}

	socket_.async_read_some(boost::asio::buffer(data_, max_length),
		boost::bind(&client::handle_read, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

void client::handle_write(const boost::system::error_code& error)
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
		cout<<"client"<<"发送数据出错：错误码="<<error.value()<<", "<<error.message()<<endl;
		//service_ptr_->stop();
		this->stop();
		return;
	}
}
#include "session.h"
#include <boost/bind.hpp>
#include <iostream>

extern boost::asio::io_service g_io_service;
session::session(boost::asio::io_service& _io_service)
	:client_socket(_io_service),server_socket(_io_service),strand_(_io_service)
{
	client_recv_count = 0;
	stopping = false;
}

session::~session(void)
{
}

void session::start()
{
	cout << "start read client" << endl;
	client_socket.async_read_some(boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_client_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::close_server()
{
	server_socket.close();
	cout << "server closed" << endl;
}

void session::close_client()
{
	client_socket.close();
	cout << "client closed" << endl;
}

void session::write_to_client(char *data, size_t size)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	//BOOST_LOG_TRIVIAL(debug) <<id_<<": "<<"向client"<<"发送"<<size<<"字节";
	boost::shared_ptr<char> data_;
	data_ = boost::shared_ptr<char>(new char[size]);
	memcpy(data_.get(), data, size);
	boost::asio::async_write(client_socket,
		boost::asio::buffer(data_.get(), size),
		strand_.wrap(boost::bind(&session::handle_client_write, shared_from_this(),
			data_,
			boost::asio::placeholders::error)
		)
	);
}

void session::handle_client_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if(error){
		if(error==boost::asio::error::operation_aborted){
			//BOOST_LOG_TRIVIAL(debug) << id_ << ": abort " << __FUNCTION__;
			return;
		}
		if(error==boost::asio::error::eof){
			close_client();
			close_server();
			return;
		}
		//BOOST_LOG_TRIVIAL(error) <<id_<<": "<<"client"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message();
		//service_ptr_->stop();
		close_client();
		close_server();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从client"<<"读取到"<<bytes_transferred<<"字节";
	client_recv_count++;
	if(client_recv_count==1)
	{
		if(bytes_transferred<3){
			//service_ptr_->stop();
			close_client();
			close_server();
			return;
		}

		if(client_buf[0]!=0x05/* || data_[1]!=0x01 || data_[2]!=0x00 || data_[3]!=0x01*/)
		{
			//service_ptr_->stop();
			close_client();
			close_server();
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
		client_buf[0] = '\x05';
		client_buf[1] = '\x00';
		write_to_client(client_buf, 2);
	}
	else if(client_recv_count==2)
	{
		if(bytes_transferred<10){
			//service_ptr_->stop();
			close_client();
			close_server();
			return;
		}

		//|VER|CMD|RSV|ATYP|DST.ADDR|DST.PORT|
		if(client_buf[0]!=0x05 || client_buf[1]!=0x01 || client_buf[2]!=0x00 || (client_buf[3]!=0x01 && client_buf[3]!=0x03))
		{
			//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"client"<<"不支持的代理协议";
			//service_ptr_->stop();
			close_client();
			close_server();
			return;
		}

		char host[128];
		char port[6];
		if(client_buf[3]==0x01) {
			sprintf(host, "%d.%d.%d.%d", (unsigned char)client_buf[4], (unsigned char)client_buf[5], 
				(unsigned char)client_buf[6], (unsigned char)client_buf[7]);
			sprintf(port, "%d", ntohs(*(unsigned short*)(client_buf+8)));
		}else if(client_buf[3]==0x03) {
			memcpy(host, client_buf + 5, client_buf[4]);
			host[client_buf[4]] = 0;
			sprintf(port, "%d", ntohs(*(unsigned short*)(client_buf + 5 + client_buf[4])));
		}

		tcp::resolver resolver(g_io_service);
		tcp::resolver::query query(tcp::v4(), host, port);
		tcp::resolver::iterator iterator = resolver.resolve(query);
		cout << "connect to " << host << ":" << port << endl;
		server_socket.async_connect(*iterator, 
			strand_.wrap(boost::bind(&session::handle_connect_server, shared_from_this(),
				boost::asio::buffer(client_buf, bytes_transferred),boost::asio::placeholders::error, iterator)
			)
		);
		return;
	}else{
		write_to_server(client_buf, bytes_transferred);
	}

	client_socket.async_read_some(boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_client_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::handle_client_write(boost::shared_ptr<char> data, const boost::system::error_code& error)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		if(error==boost::asio::error::operation_aborted)
			return;

		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"client"<<"发送数据出错：错误码="<<error.value()<<", "<<error.message();
		//service_ptr_->stop();
		close_client();
		close_server();
		return;
	}
}

void session::handle_connect_server(boost::asio::mutable_buffers_1 buffer, const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	unsigned char *p = boost::asio::buffer_cast<unsigned char*>(buffer);
	if (error)
	{
		//BOOST_LOG_TRIVIAL(debug) << id_ << ": 连接" << endpoint_iterator->host_name() << "失败";
		p[1] = 0x01;
		close_client();
		close_server();
		return;
	}
	else
	{
		//BOOST_LOG_TRIVIAL(debug) << id_ << ": 已连接到" << endpoint_iterator->host_name();
		p[1] = 0x00;
		server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
			strand_.wrap(boost::bind(&session::handle_server_read, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
			)
		);
	}
	write_to_client((char*)p, boost::asio::buffer_size(buffer));

	client_socket.async_read_some(boost::asio::buffer(client_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_client_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::write_to_server(char *data, size_t size)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"向service"<<"发送"<<size<<"字节";
	boost::asio::async_write(server_socket,
		boost::asio::buffer(data, size),
		strand_.wrap(boost::bind(&session::handle_server_write, shared_from_this(),
		boost::asio::placeholders::error)
		)
		);
}

void session::handle_server_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if(error){
		if(error==boost::asio::error::operation_aborted)
			return;
		if(error==boost::asio::error::eof){
			stopping = true;
			return;
		}
		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"读取数据出错：错误码="<<error.value()<<", "<<error.message();
		close_client();
		close_server();
		return;
	}

	//BOOST_LOG_TRIVIAL(debug)<<id_<<": "<<"从service"<<"读取到"<<bytes_transferred<<"字节";

	write_to_client(server_buf, bytes_transferred);
	server_socket.async_read_some(boost::asio::buffer(server_buf, SOCKET_RECV_BUF_LEN),
		strand_.wrap(boost::bind(&session::handle_server_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		)
	);
}

void session::handle_server_write(const boost::system::error_code& error)
{
	//BOOST_LOG_TRIVIAL(debug) << id_ << ": " << __FUNCTION__;
	if (error)
	{
		if(error==boost::asio::error::operation_aborted)
			return;

		//BOOST_LOG_TRIVIAL(error)<<id_<<": "<<"service"<<"向服务器发送数据出错：错误码="<<error.value()<<", "<<error.message();
		close_client();
		close_server();
		return;
	}
}
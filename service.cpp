#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "service.h"

service::service(boost::asio::io_service& io_service, short port)
	: io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
{
	session_id = 0;
	session_ptr.reset(new session(io_service, session_id++));
}

service::~service()
{

}

void service::start()
{
	async_accept();
}

void service::async_accept()
{
	acceptor_.async_accept(
		session_ptr->client_socket,
		boost::bind(&service::handle_accept, shared_from_this(), boost::ref(session_ptr->client_socket), _1)
	);
}

void service::handle_accept(tcp::socket &client_socket, const boost::system::error_code& error)
{
	if (error)
	{
		BOOST_LOG_TRIVIAL(error) << error.message();
		session_ptr->stop();
		async_accept();
		return;
	}

	std::string remote_addr;
	try {
		remote_addr = client_socket.remote_endpoint().address().to_string();
		unsigned short port = client_socket.remote_endpoint().port();
		remote_addr += ":";
		remote_addr += boost::lexical_cast<std::string>(port);
	}
	catch (const std::exception& e) {
		BOOST_LOG_TRIVIAL(error) << e.what();
		session_ptr->stop();
		async_accept();
		return;
	}

	BOOST_LOG_TRIVIAL(debug) << "[" << session_ptr->get_session_id() << "] " << remote_addr << " connected";
	session_ptr->start();
	session_ptr.reset(new session(io_service_, session_id++));

	async_accept();
}
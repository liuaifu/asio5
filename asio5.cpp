#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/log/utility/init/to_file.hpp>
#include <boost/log/utility/init/to_console.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/filters.hpp>
#include "session.h"
#include "stdlib.h"

using namespace std;
using boost::asio::ip::tcp;
boost::asio::io_service g_io_service;

class server
{
public:
	server(boost::asio::io_service& io_service, short port)
		: io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		start_accept();
	}

private:
	void start_accept()
	{
		static int id = 0;
		id++;

		boost::shared_ptr<client> client_ptr_ = session_.newSession();
		acceptor_.async_accept(client_ptr_->socket(),
			boost::bind(&server::handle_accept, this, client_ptr_, _1));
	}

	void handle_accept(boost::shared_ptr<client> client_ptr_, const boost::system::error_code& error)
	{
		if (!error)
		{
			session_.start();
		}
		else
		{
			cout<<__FUNCTION__<<"´íÎó£º"<<error<<endl;
		}

		start_accept();
		client_ptr_.reset();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	//tcp::socket socket_;
	session session_;

};

int main(int argc, char* argv[])
{
	boost::log::init_log_to_console(std::clog, boost::log::keywords::format = "%TimeStamp%: %_%");

	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: asio5 <port>\n";
			return 1;
		}

		using namespace std; // For atoi.
		server s(g_io_service, atoi(argv[1]));
		cout<<"listen on 0.0.0.0:"<<argv[1]<<endl;
		g_io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	system("pause");
	return 0;
}

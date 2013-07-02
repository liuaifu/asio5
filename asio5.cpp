#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#include "session.h"

#if !defined (_WIN32) && !defined (_WIN64)
#include <unistd.h>
#else
//#include <windows.h>
#endif

using namespace std;
using namespace boost;
using boost::asio::ip::tcp;
boost::asio::io_service g_io_service;

class server
{
public:
	server(asio::io_service& io_service, short port)
		: io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		session_ptr.reset(new session());
		start_accept();
	}

private:
	void start_accept()
	{
		static int id = 0;
		id++;

		boost::shared_ptr<client> client_ptr_ = session_ptr->newSession();
		acceptor_.async_accept(client_ptr_->socket(),
			boost::bind(&server::handle_accept, this, client_ptr_, _1));
	}

	void handle_accept(boost::shared_ptr<client> client_ptr_, const boost::system::error_code& error)
	{

		if (!error &&client_ptr_->socket().remote_endpoint().address().to_string()!="")
		{
			cout << client_ptr_->socket().remote_endpoint().address().to_string() + " connected" << endl;
			session_ptr->start();
		}
		else
		{
			session_ptr->stop(client_ptr_);
			cout << __FUNCTION__ << "´íÎó£º" << error << endl;
		}

		start_accept();
		client_ptr_.reset();
	}

	asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	boost::shared_ptr<session> session_ptr;

};

unsigned int get_core_count()
{
	unsigned count = 1;
#if !defined (_WIN32) && !defined (_WIN64)
	count = sysconf(_SC_NPROCESSORS_CONF);	//_SC_NPROCESSORS_ONLN
#else
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	count = si.dwNumberOfProcessors;
#endif  
	return count;
}

int main(int argc, char* argv [])
{
	cout << "asio5 v0.2" << endl;
	cout << "laf163@gmail.com" << endl;
	vector<boost::shared_ptr<thread> > threads;

	if (argc != 2)
	{
		std::cerr << "Usage: asio5 <port>\n";
		return 1;
	}

	int port;
	try{
		port = lexical_cast<int>(argv[1]);
	}
	catch (bad_lexical_cast &e){
		cout << "port error! " << e.what() << endl;
		return 1;
	}
	boost::shared_ptr<server> server_ptr(new server(g_io_service, port));
	cout << "work thread count is " << get_core_count() << endl;
	cout << "listening on 0.0.0.0:" << port << endl;
	for (unsigned int i = 0; i < get_core_count() - 1; i++){
		threads.push_back(boost::shared_ptr<thread>(new thread(bind(&asio::io_service::run, &g_io_service))));
	}
	g_io_service.run();

	return 0;
}

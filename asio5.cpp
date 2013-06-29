#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "session.h"

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
		if (!error)
		{
			session_ptr->start();
		}
		else
		{
			cout<<__FUNCTION__<<"´íÎó£º"<<error<<endl;
		}

		start_accept();
		client_ptr_.reset();
	}

	asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	shared_ptr<session> session_ptr;

};

int main(int argc, char* argv[])
{
	cout<<"asio5 v0.2"<<endl;
	cout<<"laf163@gmail.com"<<endl;
	vector<shared_ptr<thread> > threads;
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: asio5 <port>\n";
			return 1;
		}

		int port;
		try{		
			lexical_cast<int>(argv[1]);
		}
		catch(bad_lexical_cast &e){
			cout<<"port error! "<<e.what()<<endl;
			return 1;
		}
		shared_ptr<server> server_ptr( new server(g_io_service, atoi(argv[1])) );
		cout<<"listen on 0.0.0.0:"<<argv[1]<<endl;
		for(int i=0;i<4;i++){
			threads.push_back(shared_ptr<thread>( new thread(bind(&asio::io_service::run, &g_io_service)) ));
		}
		g_io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}

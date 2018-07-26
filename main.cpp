#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
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

class server
{
public:
	server(asio::io_service& io_service, short port)
		: io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		session_ptr.reset(new session(io_service));
		start_accept();
	}

private:
	void start_accept()
	{
		static int id = 0;
		id++;

		acceptor_.async_accept(session_ptr->client_socket,
			boost::bind(&server::handle_accept, this, boost::ref(session_ptr->client_socket), _1));
	}

	void handle_accept(tcp::socket &client_socket, const boost::system::error_code& error)
	{
		if (error)
		{
			cout << error << endl;
			session_ptr->close_client();
			session_ptr->close_server();
			start_accept();
			return;
		}

		std::string remote_addr;
		try {
			remote_addr = client_socket.remote_endpoint().address().to_string();
			unsigned short port = client_socket.remote_endpoint().port();
			remote_addr += boost::lexical_cast<std::string>(port);
		}
		catch(const std::exception& e) {
			cout << e.what() << endl;
			session_ptr->close_client();
			session_ptr->close_server();
			start_accept();
			return;
		}

		cout << remote_addr << " connected" << endl;
		session_ptr->start();
		session_ptr.reset(new session(io_service_));

		start_accept();
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
	cout << "asio5 v0.4" << endl;
	cout << "laf163@gmail.com" << endl;

	using namespace boost::program_options;
	//声明需要的选项
	options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("port", value<int>()->default_value(7070), "listen port")
		;

	variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);
	notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	vector<boost::shared_ptr<boost::thread> > threads;

	int port = vm["port"].as<int>();
	boost::asio::io_service io_service_;
	boost::shared_ptr<server> server_ptr(new server(io_service_, port));
	cout << "work thread count is " << get_core_count() << endl;
	cout << "listening on 0.0.0.0:" << port << endl;
	for (unsigned int i = 0; i < get_core_count() - 1; i++){
		threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&asio::io_service::run, &io_service_))));
	}
	io_service_.run();

	return 0;
}

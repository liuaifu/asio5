#if !defined (_WIN32) && !defined (_WIN64)
#include <unistd.h>
#else
#include "targetver.h"
#endif

#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>



#include "service.h"

using namespace boost;
using boost::asio::ip::tcp;


void init_log(boost::log::trivial::severity_level log_level)
{
	boost::log::add_console_log(std::cout, boost::log::keywords::format = (
		boost::log::expressions::stream
		<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%H:%M:%S.%f")
		<< " <" << boost::log::trivial::severity
		<< "> " << boost::log::expressions::smessage)
	);
	boost::log::add_file_log
	(
		boost::log::keywords::file_name = "log/asio5_%Y%m%d_%N.log",
		boost::log::keywords::rotation_size = 50 * 1024 * 1024,
		boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
		boost::log::keywords::auto_flush = true,
		boost::log::keywords::open_mode = std::ios::app,
		boost::log::keywords::format = (
			boost::log::expressions::stream
			<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			<< "|" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
			<< "|" << boost::log::trivial::severity
			<< "|" << boost::log::expressions::smessage
			)
	);
	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= log_level
	);
	boost::log::add_common_attributes();
}

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

int main(int argc, char* argv[])
{
	using namespace boost::program_options;
	//声明需要的选项
	options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("port", value<int>()->default_value(7070), "listen port")
		("log-level", value<int>()->default_value(1), "0:trace,1:debug,2:info,3:warning,4:error,5:fatal")
		;

	variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);
	notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	int log_level = vm["log-level"].as<int>();
	if (log_level < 0 || log_level > 5) {
		std::cerr << "log-level invalid" << std::endl;
		std::exit(-1);
	}

	init_log((boost::log::trivial::severity_level)log_level);

	BOOST_LOG_TRIVIAL(info) << "--------------------------------------------------";
	BOOST_LOG_TRIVIAL(info) << "asio5 v0.5";
	BOOST_LOG_TRIVIAL(info) << "laf163@gmail.com";
	BOOST_LOG_TRIVIAL(info) << "compiled at " << __DATE__ << " " << __TIME__;

	std::vector<boost::shared_ptr<boost::thread> > threads;

	int port = vm["port"].as<int>();
	boost::asio::io_service io_service_;
	boost::shared_ptr<service> service_ptr(new service(io_service_, port));
	service_ptr->start();
	BOOST_LOG_TRIVIAL(info) << "work thread count: " << get_core_count();
	BOOST_LOG_TRIVIAL(info) << "listening on 0.0.0.0:" << port;
	for (unsigned int i = 0; i < get_core_count() - 1; i++) {
		threads.push_back(
			boost::shared_ptr<boost::thread>(
				new boost::thread(
					boost::bind(&asio::io_service::run, &io_service_)
				)
			)
		);
	}
	io_service_.run();

	return 0;
}

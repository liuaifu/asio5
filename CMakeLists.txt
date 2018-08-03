cmake_minimum_required(VERSION 2.8)
PROJECT(asio5)
ADD_EXECUTABLE(asio5
	main.cpp
	service.cpp
	session.cpp
	session_http1.cpp
	session_http2.cpp
	session_socks4.cpp
	)

find_package(Boost
	1.42.0
	REQUIRED
	COMPONENTS system thread program_options log
	)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(asio5 ${Boost_LIBRARIES})
endif(Boost_FOUND)

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

if(ENCRYPT)
	ADD_DEFINITIONS(-DENCRYPT)
endif(ENCRYPT)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)

find_package(Boost
	1.54.0
	REQUIRED
	COMPONENTS system thread program_options log
	)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(asio5 ${Boost_LIBRARIES})
endif(Boost_FOUND)

cmake_minimum_required(VERSION 2.8)
PROJECT(asio5)
ADD_EXECUTABLE(asio5
	asio5.cpp
	session.cpp
	)

find_package(Boost
	1.42.0
	REQUIRED
	COMPONENTS system thread
	)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(asio5 ${Boost_LIBRARIES})
endif(Boost_FOUND)

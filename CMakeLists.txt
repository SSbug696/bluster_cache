cmake_minimum_required(VERSION 2.8)
project(bluster_cache)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -flto -O2 -fstrict-aliasing -Wall -pedantic")

if(UNIX AND NOT APPLE AND NOT WIN32)
    set(LINUX TRUE)
endif()

if(LINUX) 
    set(SOURCES_FILES qcache.cpp epoll_server.cpp server_init.cpp rrlist.cpp log.cpp)
elseif(UNIX AND NOT LINUX)
    set(SOURCES_FILES qcache.cpp kqueue_server.cpp server_init.cpp rrlist.cpp log.cpp)
else()
    message(STATUS ">>>  Current OS is not supported")
endif()

add_executable (src ${SOURCES_FILES})
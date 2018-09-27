cmake_minimum_required(VERSION 2.8)
project(bluster_cache)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORCE_INLINES -std=c++14 -pthread -flto -O2 -fstrict-aliasing -Wall -pedantic")

if(UNIX AND NOT APPLE AND NOT WIN32)
    set(LINUX TRUE)
endif()

if(LINUX) 
    set(SOURCES_FILES rrlist.cpp log.cpp qcache.cpp epoll_server.cpp server_init.cpp server.cpp config_reader.cpp )
elseif(UNIX AND NOT LINUX)
    set(SOURCES_FILES rrlist.cpp log.cpp qcache.cpp kqueue_server.cpp server_init.cpp server.cpp config_reader.cpp)
else()
    message(STATUS ">>>  Current OS is not supported")
endif()

add_executable (src ${SOURCES_FILES})

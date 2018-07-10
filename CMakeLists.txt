cmake_minimum_required(VERSION 2.8)
project(bluster_cache)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -flto -O2 -fstrict-aliasing -Wall -pedantic")

set(SOURCES_FILES qcache.cpp kqueue_server.cpp server_init.cpp rrlist.cpp log.cpp)
add_executable(src ${SOURCES_FILES})


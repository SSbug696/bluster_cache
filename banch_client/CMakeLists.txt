cmake_minimum_required(VERSION 2.8)
project(bluster_cache)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall")

if(UNIX)
    set(SOURCES_FILES client.cpp)
else()
    message(STATUS ">>>  Current OS is not supported")
endif()

add_executable (banch ${SOURCES_FILES})
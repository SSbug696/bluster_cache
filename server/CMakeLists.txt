
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORCE_INLINES -std=c++11 -pthread -flto -O2 -fstrict-aliasing -Wall -pedantic")

if(UNIX AND NOT APPLE AND NOT WIN32)
    set(LINUX TRUE)
endif()

message(CMAKE_CXX_FLAGS)

if(LINUX) 
    set(SOURCES_FILES rrlist.cpp log.cpp qcache.cpp epoll_server.cpp server_init.cpp server.cpp config_reader.cpp )
elseif(UNIX AND NOT LINUX)
    set(SOURCES_FILES rrlist.cpp log.cpp qcache.cpp kqueue_server.cpp server_init.cpp server.cpp config_reader.cpp)
else()
    message(STATUS ">>>  Current OS is not supported")
endif()

add_executable (src ${SOURCES_FILES})

#ifdef __APPLE__
  #include "kqueue_server.h"
#elif __linux__
  #include "epoll_server.h"
#elif __UNIX__
  #include "kqueue_server.h"
#endif

#define CACHE_POOL_SIZE 10000

#include <iostream>

int main (int argc, char * argv[]) {
  Server server(CACHE_POOL_SIZE);
  std::clog << "Server is running on "
            << argv[1] << " port with pool "
            << CACHE_POOL_SIZE << " bytes \n" << std::endl;

  server.init(argv[1]);
}
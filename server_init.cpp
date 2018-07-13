
#ifdef __APPLE__
  #include "kqueue_server.h"
#elif
  // In developing
  // #include "epoll_server.h"
#endif

#define CACHE_POOL_SIZE 10000
#include <iostream>


int main (int argc, char * argv[]) {
  Server server(CACHE_POOL_SIZE);
  printf("Server is running on %s port with pool %d bytes \n", argv[1], CACHE_POOL_SIZE);
  server.init(argv[1]);
}
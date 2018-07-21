#include "server.h"
#include <iostream>

#define CACHE_POOL_SIZE 10000

int main (int argc, char * argv[]) {
  Server server(CACHE_POOL_SIZE);
  std::clog << "Server is running on "
            << argv[1] << " port with pool "
            << CACHE_POOL_SIZE << " bytes \n" << std::endl;

  server.init(argv[1]);
}
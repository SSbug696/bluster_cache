
#ifdef __APPLE__
 #include "kqueue_server.h"
#elif
  #include "epoll_server.h"
#endif

#include <iostream>

int main (int argc, char * argv[]) {

  Server server(1000);

  std::cout << argv[1] << std::endl;

  server.init(argv[1]);
}
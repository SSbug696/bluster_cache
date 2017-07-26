
#include "epoll_server.cpp"
#include <iostream>

int main (int argc, char * argv[]) {

  Server server(1000);

  std::cout << argv[1] << std::endl;

  server.init(argv[1]);
}
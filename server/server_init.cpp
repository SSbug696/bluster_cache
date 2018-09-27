#include "server.h"
#include "config_reader.h"
#include <iostream>
#include <map>

int main (int argc, char * argv[]) {
  CfgReader cfg_reader;
  std::map<std::string, size_t> & cfg = cfg_reader.read();
  
  Server server(cfg["CACHE_POOL_SIZE"]);
  
  std::clog << "Server is running on "
            << cfg["PORT"] << " port with pool "
            << cfg["CACHE_POOL_SIZE"] << " bytes \n" << std::endl;

  server.init(cfg["PORT"]);
}
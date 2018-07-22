#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

class CfgReader {
  std::map<std::string, size_t> kv;
  bool is_number(const std::string &);

  std::string source_line;
  std::stringstream key;
  std::stringstream val;

public:
  bool validator();
  std::map<std::string, size_t> & read();
};
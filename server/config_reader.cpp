#include "config_reader.h"

bool CfgReader::is_number(const std::string& s) {
  return !s.empty() && std::find_if(s.begin(), 
    s.end(), [](char c) { return !std::isdigit(c) && c!=' '; }) == s.end();
}

bool CfgReader::validator() {
  std::map<std::string, size_t> assign_map;
  assign_map["CACHE_POOL_SIZE"] = 4096 * 4096;
  assign_map["PORT"] = 0xfffe;

  size_t keys_counter = assign_map.size();
  
  for(auto &i: kv) {
    if(assign_map.count(i.first)) {
      keys_counter --;
    }
  }

  if(keys_counter > 0) {
    return false;
  } else return true;
}

std::map<std::string, size_t> & CfgReader::read() {
  std::fstream f("cfg.ini");
  if(f.is_open()) {
    while(!f.eof()) {
      getline(f, source_line);

      int i = 0;
      bool is_key = true;
      while(i < source_line.length()) {
        if(source_line[i] == ' ') {
          if(is_key == false) break;
          is_key = false;
        }

        if(is_key) {
          key << source_line[i];
        } else {
          val << source_line[i];
        }
        
        i++;
      }

      std::string str(val.str());
      if(is_number(str)) {
        kv[key.str()] = stoi(str);
      }

      source_line.clear();
      key.str("");
      val.str("");
    }
  }

  if( !validator() ) {
    std::cerr << "Error of reading config file" << std::endl;
    exit(EXIT_FAILURE);
  }

  return kv;
}
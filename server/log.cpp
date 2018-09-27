#include "log.h"

std::ostringstream & Log::get(log_types level) {
  LVL_REPORTING = (log_types)LOG_TYPE;
  msg_lvl = level;
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  os << "- " << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  os << " " << get_enum_str(level) << " ";
  return os;
}

Log::~Log() {
  if(LVL_REPORTING <= msg_lvl) {
    if(CIO_ENABLED) {
      std::cout << os.str() << std::endl;
    }

    if(FIO_FLAG) {
      if(msg_lvl >= LWARN) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream file_name;
        file_name << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H:%m") << ".log";

        std::ofstream fd;
        fd.open(file_name.str(), std::ofstream::in | std::ofstream::app);
        if(fd.is_open()) {
          fd << os.str();
          fd.close();
        } else {
          std::clog << "Can't create log file" << std::endl;
        }
      }
    }
  }
}

std::string Log::get_enum_str(log_types n) {
  std::string type;
  
  switch(n) {
    case LINFO:
      type = "INFO";
    break;

    case LDEBUG:
      type = "DEBUG";
    break;     

    case LWARN:
      type = "WARNING";
    break;

    case LERR:
      type = "ERROR";  
    break;
  }
  
  return type;
}
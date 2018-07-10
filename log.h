#include <iostream>
#include <istream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <fstream>

enum log_types { LDEBUG, LINFO, LWARN, LERR };

class Log {
  protected:
    std::ostringstream os;

  private:
    log_types msg_lvl;
    // Get string from current level notification
    std::string get_enum_str(log_types);

    // Alert lvl
    const log_types LVL_REPORTING = LDEBUG;
    // File I/O resolution for file ops
    const bool FIO_FLAG = true; 
    // File I/O resolution for command prompt
    const bool CIO_ENABLED = true;

  public:
    ~Log();
    std::ostringstream & get(log_types level = LDEBUG);
};
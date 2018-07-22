#include <sys/epoll.h>
#include <map>
#include <string>
#include "cache_options.h"

class Multiplexor {
protected:
  int local_s, s, efd;
  epoll_event event;
  epoll_event events[MAXEVENTS];
};

#include <sys/event.h>
#include <map>
#include <string>
#include "cache_options.h"

class Multiplexor {
protected:
  int sfd, s, kq;
  struct kevent ev;
  struct kevent ev_set[MAXEVENTS];
};

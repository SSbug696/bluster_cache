#include <sys/epoll.h>

#define MAX_BUFFER_SIZE 2048
#define MAXEVENTS 1024
#define WORKERS_POOL 4
#define MAX_LEN_PREFIX 10

class Multiplexor {
protected:
  int local_s, s, efd;
  epoll_event event;
  epoll_event events[MAXEVENTS];
};

#include <sys/event.h>

#define MAX_BUFFER_SIZE 2048
#define MAXEVENTS 1024
#define WORKERS_POOL 4
#define MAX_LEN_PREFIX 10

class Multiplexor {
protected:
  int sfd, s, kq;
  struct kevent ev;
  struct kevent ev_set[MAXEVENTS];
};

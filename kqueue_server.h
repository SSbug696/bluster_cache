#include <sys/event.h>
#include "main_server.h"

class Server : private MainServer {
  int sfd, s, kq;
  struct kevent ev;
  struct kevent ev_set[MAXEVENTS];

  int make_socket_non_blocking(int);
  int create_and_bind(char *);
  void init_workers_pool();
  void do_task();
  inline void rm_fd(size_t);
  inline void clear_buffer(size_t);
  inline int get_msg_sz(char[], const size_t);
  inline int get_len_prefix(int);
  void notify_shed();

public:
  Server(size_t);
  ~Server() {
    delete _round_queue;
    delete _cache;
  }

  int init(char *);
};

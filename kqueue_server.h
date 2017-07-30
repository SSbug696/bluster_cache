#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <list>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <atomic>

#include "cache.h"

class Server {
  const size_t MAXEVENTS = 64;
  const size_t WORKERS_POOL = 1;
  const size_t MAX_BUFFER_SIZE = 8000000;
  enum  actions { UNKNOWN = 0, ERR, SET, GET, DEL, EXIST, FLUSH, SIZE };
  std::atomic<long long> atomic_lock_nr;

  // Minimum command length
  std::map<std::string, size_t> _min_commands_len;
  std::map<std::string, size_t> _assoc_dict_commands;

  // Task struct
  struct task_struct {
    std::string command;
    size_t handle_client;
  };

  int sfd, s;
  int efd;

  struct kevent events_set;
  struct kevent events_list[32];

  QCache * cache;
  std::vector<std::thread> _thread_pool;
  std::queue<task_struct *> _request_queue;

  std::mutex _mutex_rw;
  std::mutex _mutex_cache;
  std::mutex _mutex_queue;

  ssize_t _recv_bytes_count;
  char _buffer_recv[100000];

  int make_socket_non_blocking(int);
  int create_and_bind (char *);
  void intitWorkersPool();
  void data();

public:
  Server(size_t);
//   int conn_index(int);
//   int conn_add(int);
//   int conn_delete(int);

  ~Server() {
    delete cache;
    //free(events);
  }

  int init(char *);
};

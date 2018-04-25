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
#include <map>

#include "cache.h"


#define MAX_BUFFER_SIZE 4096
#define MAXEVENTS 10
#define WORKERS_POOL 1



class Server {
  enum  actions { UNKNOWN = 0, ERR, SET, GET, DEL, EXIST, FLUSH, SIZE };

  std::condition_variable _writer_cond, _recv_cond;
  std::atomic<bool> _atomic_lock_nr;
  // Minimum command length
  std::map<std::string, size_t> _min_commands_len;
  std::map<std::string, size_t> _assoc_dict_commands;
  std::queue<size_t>  _request_queue;
  
  // Task struct
  struct task_struct {
    std::stringstream command;
    char send_buffer[MAX_BUFFER_SIZE];
    size_t offset;
    short int terminate;
  };

  std::map<size_t, task_struct *> tasks;

  int sfd, s;
  int efd;

  struct kevent events_set;
  struct kevent events_list[MAXEVENTS];

  QCache * cache;
  std::vector<std::thread> _thread_pool;

  std::mutex _mutex_rw;
  std::mutex _mutex_cache;
  std::mutex _mutex_queue, _mutex_task;

  ssize_t _recv_bytes_count;
  char _buffer_recv[MAX_BUFFER_SIZE];

  int make_socket_non_blocking(int);
  int create_and_bind (char *);
  void initWorkersPool();
  void data();
  void rmFD(size_t);
  void clearBuffer(size_t);

public:
  Server(size_t);
  ~Server() {
    delete cache;
  }

  int init(char *);
};

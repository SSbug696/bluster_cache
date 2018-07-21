#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <list>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <time.h>
#include <iterator>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <utility>
#include "qcache.h"
#include "rrlist.h"
#include "log.h"

#ifdef __APPLE__
  #include "multiplexor_kqueue.h"
#elif __linux__
  #include "multiplexor_epoll.h"
#elif __UNIX__
  #include "multiplexor_kqueue.h"
#endif

class Server: Multiplexor {
protected:
  enum actions {
    UNKNOWN = 0,
    ERR,
    SET,
    GET,
    DEL,
    EXIST,
    FLUSH,
    SIZE
  };

  std::condition_variable _writer_cond;
  std::atomic<bool> _is_locked, _notify_shed;
  //bool _notify_shed;
  std::map<std::string, size_t> _assoc_dict_commands;
  char nb[MAX_LEN_PREFIX];
  // Round-robin queue for request notifications pipeline
  RRList *_round_queue;
  // LRU backand
  QCache *_cache;

  // Task struct
  struct task_struct {
    // Activity flag current task, not multiple context with current task id
    bool processing;
    // Flag of task id presence in the pipeline
    int send_bytes;
    int recv_bytes;
    // Status of current task
    //std::atomic<bool>
    int status;
    //std::atomic<size_t>
    size_t in_round_counter;
    char send_buffer[MAX_BUFFER_SIZE];
    char command[MAX_BUFFER_SIZE];
  };

  // Base struct of connection status
  std::map<size_t, task_struct *> tasks;
  // Pool of workers. One sheduler and writers
  std::vector<std::thread> _thread_pool;
  std::mutex _mutex_rw, _mutex_shed, _mutex_cache;
  ssize_t _recv_bytes_count;
  char _buffer_recv[MAX_BUFFER_SIZE];

  int make_socket_non_blocking(int);
  int create_and_bind(char *);
  void init_workers_pool();
  void do_task();
  inline void rm_fd(size_t);
  inline void clear_buffer(size_t);
  inline int get_msg_sz(char[], const size_t);
  inline int get_len_prefix(int);

public:
  Server(size_t);
  ~ Server() {
    delete _round_queue;
    delete _cache;
  }

  int init(char *);
};
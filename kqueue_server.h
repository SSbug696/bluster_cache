#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/tcp.h>
#include <sys/event.h>
#include <sys/socket.h>
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

#define MAX_BUFFER_SIZE 2048
#define MAXEVENTS 1024
#define WORKERS_POOL 3
#define CHUNK_SIZE 100
#define TIME_DIFF 0xff
#define MAX_KEY_SZ 800
#define MAX_VALUE_SZ 1000
#define MAX_LEN_PREFIX 10

class Server {
  enum  actions { UNKNOWN = 0, ERR, SET, GET, DEL, EXIST, FLUSH, SIZE };

  std::condition_variable _writer_cond;
  std::atomic<bool> _is_locked, _notify_shed;
  //bool _notify_shed;
  std::map<std::string, size_t> _assoc_dict_commands;  
  char nb[MAX_LEN_PREFIX];
  // Round-robin queue for request notifications pipeline
  RRList *_round_queue;
  // LRU backand
  QCache * _cache;
  
  // Task struct
  struct task_struct {
    // Activity flag current task, not multiple context with current task id
    bool processing;
    // Flag of task id presence in the pipeline
    int send_bytes;
    int recv_bytes;
    // Status of current task
    std::atomic<bool> status;
    std::atomic<size_t> in_round_counter;
    char send_buffer[MAX_BUFFER_SIZE];
    char command[MAX_BUFFER_SIZE];
  };

  // Base struct of connection status
  std::map<size_t, task_struct *> tasks;

  int sfd, s;
  int efd;
  int kq;

  struct kevent ev;
  struct kevent ev_set[MAXEVENTS];

  // Pool of workers. One sheduler and writers
  std::vector<std::thread> _thread_pool;

  std::mutex _mutex_rw, _mutex_shed, _mutex_cache;

  ssize_t _recv_bytes_count;
  char _buffer_recv[MAX_BUFFER_SIZE];

  int make_socket_non_blocking(int);
  int create_and_bind (char *);
  void init_workers_pool();
  void do_task();
  inline void rm_fd(size_t);
  inline void clear_buffer(size_t);
  inline int get_msg_sz(char [], const size_t);
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

#include "kqueue_server.h"

Server::Server(size_t max_pool_sz) {
  _cache = new QCache(max_pool_sz);
  // Round-robin queue for sheduler
  _round_queue = new RRList();

  init_workers_pool();

  _assoc_dict_commands["set"]   = SET;
  _assoc_dict_commands["get"]   = GET;
  _assoc_dict_commands["exist"] = EXIST;
  _assoc_dict_commands["del"]   = DEL;
  _assoc_dict_commands["flush"] = FLUSH;
  _assoc_dict_commands["size"]  = SIZE;
}

void Server::do_task() {
  // For request result
  std::string result;
  // Source command
  std::string command_line;
  std::string tmp_str;
  std::string key;
  std::string value;
  std::vector<std::string> assets;
  std::vector<std::string> multi_parts;
  std::vector<std::string> string_parts;

  // Buffer for part of command request
  char tmp_buffer[MAX_BUFFER_SIZE];
  short int counter = 0;
  int sz = 0;
  int uid = 0;

  size_t sz_inserts;
  size_t expire = 0;
  size_t COMMAND_ID;
  size_t chr_counter = 0;

  task_struct * t_ptr;
  
  bool is_quote_substring = false;
  bool is_pushed = false;
  char buffer_source[MAX_BUFFER_SIZE];
  int chunk_offset = 0;
  int is_flag = 1;
  int wc = 0;
  
  memset(buffer_source, 0, MAX_BUFFER_SIZE);
  std::unique_lock<std::mutex> mlock_rt(_mutex_rw);

  while(1) {
    memset(tmp_buffer, 0, MAX_BUFFER_SIZE);

    chunk_offset = 0;
    is_flag = 1;
    wc = 0;

    multi_parts.clear();
    is_quote_substring = false;
    is_pushed = false;

    // Awaiting wake up
    _writer_cond.wait(mlock_rt);

    uid = _round_queue->next_slice(command_line);
    if(uid == -1) {
      _notify_shed = false;
      continue;
    }

    // To shift pointer
    t_ptr = tasks[uid];
    _notify_shed = false;
    sz = command_line.size();

    // Detecting multiple inserts
    if(command_line[0] == '[') {
      for(size_t i = 1; i < sz; i ++) {
        // Trigger for double quotes
        if(command_line[i] == '"' && command_line[i - 1] != '\\') {
          is_quote_substring = !is_quote_substring;
        }

        // Split by separator
        if((command_line[i] == ',' && is_quote_substring == false) || (i == sz - 1)) {
          tmp_str = tmp_buffer;
          multi_parts.push_back(tmp_str);
          // Clear to message length
          memset(tmp_buffer, 0, counter);
          counter = 0;
          is_quote_substring = false;
          continue;
        }

        tmp_buffer[counter] = command_line[i];
        counter ++;
      }
    } else {
      multi_parts.push_back(command_line);
    }

    sz_inserts = multi_parts.size();
    counter = 0;

    for(size_t k = 0; k < sz_inserts; k ++) {
      // Set default command value
      command_line = multi_parts[k];
      sz = command_line.size();
      
      memset(tmp_buffer, 0, MAX_BUFFER_SIZE);

      for(size_t i = 0; i < sz; i ++) {
        // Isn't whitespace
        if(command_line[i] != ' ') {
          // Trigger for define end substring and push substring to buffer
          if(command_line[i] == '"') {
            if(command_line[i - 1] != '\\') {
              is_quote_substring = !is_quote_substring;
            }
          }

          // Remove double quote for command and key
          if((command_line[i] == '"') && counter <= 1) {
            // pass
          } else {
            tmp_buffer[chr_counter ++] = command_line[i];
          }
        } else {
          if(is_quote_substring == true) {
            tmp_buffer[chr_counter ++] = command_line[i];
          }
          
          if(chr_counter > 0 && is_quote_substring == false) {
            is_pushed = true;
          }
        }

        // If EOF of request
        if( is_pushed == true || i >= sz - 1 ) {
          is_quote_substring = false;
          tmp_str = tmp_buffer;

          string_parts.push_back(tmp_str);

          memset(tmp_buffer, 0, chr_counter);
          chr_counter = 0;
          counter ++;
          is_pushed = false;
        }
      }

      //  Get command ID
      COMMAND_ID = _assoc_dict_commands[ string_parts[0] ];

      for(int i = 1; i < counter; i ++) {
        switch(i) {
          case 1:
            key = string_parts[i];
          break;

          case 2:
            value = string_parts[i];
          break;

          default:
            assets.push_back(string_parts[i]);
          break;
        }
      }

      memset(tmp_buffer, 0, sizeof(tmp_buffer));

      _mutex_cache.lock();

      switch(COMMAND_ID) {
        case SET:
          if(assets.size() > 0) {
            std::string exp_label = assets.front();
            if( strspn( exp_label.c_str(), "0123456789" ) == exp_label.size() ) {
              expire = atoi( exp_label.c_str() );
            }
          }

          // If expire is not defined
          if(expire != 0) {
            result = _cache->put(std::move(key), std::move(value), expire);
          } else {
            result = _cache->put(std::move(key), std::move(value));
          }
        break;

        case GET:
          result = _cache->get(std::move(key));
        break;

        case DEL:
          result = _cache->del(std::move(key));
        break;

        case FLUSH:
          result = _cache->flush();
        break;

        case SIZE:
          result = _cache->size();
        break;

        case EXIST:
          result = _cache->exist(std::move(key));
        break;

        case UNKNOWN:
          result = "(unknown)";
        break;

        case ERR:
          result = "(err)";
        break;

        default:
          result = "(err)";
        break;
      }

      _mutex_cache.unlock();

      // Clear string vector
      string_parts.clear();
      // Clear assets args
      assets.clear();
      // Reset expire
      expire = 0;
      counter = 0;
    }

    sz = result.size();
    memset(buffer_source + sz, 0, MAX_BUFFER_SIZE - sz);
    result.copy(buffer_source, sz);

    //While until buffer isn't flushed and don't exist error
    while(t_ptr->status && is_flag) {
      // Write data by chunks with chunk_offset
      wc = write(uid, buffer_source + chunk_offset,  sz - chunk_offset);

      if(wc < 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
          continue;
        } else {
          t_ptr->status = false;
        }
      } else if(wc == 0) {
        t_ptr->status = false;
      } else {
        chunk_offset += wc;
        if(chunk_offset == sz) break;
      }
    }

    t_ptr->in_round_counter --;
    memset(buffer_source + sz, 0,  chunk_offset);
    sz = 0;
  }
}

void Server::init_workers_pool() {
  for(size_t i = 0; i < WORKERS_POOL; ++i) {
    _thread_pool.push_back(std::thread(&Server::do_task, this));
  }

  for_each(_thread_pool.end(), _thread_pool.end(), [](std::thread &t){ t.detach(); });
}

int Server::make_socket_non_blocking(int sfd) {
  int flags = fcntl(sfd, F_GETFL, 0);
  if(flags < 0) {
    Log().get(LERR) << "FCNTL error";
    exit(EXIT_FAILURE);
  }

  flags |= O_NONBLOCK;
  fcntl(sfd, F_SETFL, flags);

  struct so_linger {
    int l_onoff;
    int l_linger;
  } linger;

  int enable_flag = 1;
  linger.l_onoff = 0;
  linger.l_linger = 0;

  setsockopt(sfd, SOL_SOCKET, SO_LINGER,  &linger, sizeof(linger));
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable_flag, sizeof(enable_flag));
  setsockopt(sfd, SOL_SOCKET, TCP_NODELAY, &enable_flag, sizeof(enable_flag));
  return 0;
}

int Server::create_and_bind(char * port) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sfd;

  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  s = getaddrinfo(NULL, port, &hints, &result);
  if(s != 0) {
    Log().get(LERR) << " getaddrinfo:" << gai_strerror(s);
    exit(EXIT_FAILURE);
  }

  for(rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) {
      Log().get(LERR) << "Error of NET family";
      exit(EXIT_FAILURE);
    }

    s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
    if(s == 0) {
      /* We managed to bind successfully! */
      break;
    }

    close(sfd);
  }

  if (rp == NULL) {
    Log().get(LERR) << "Could not bind";
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(result);
  return sfd;
}

void Server::clear_buffer(size_t fd) {
  delete tasks[fd];
  tasks.erase(fd);
}

void Server::rm_fd(size_t fd) {
  memset(_buffer_recv, 0, MAX_BUFFER_SIZE);
  close(fd);
}

int Server::init(char * port) {
  struct sockaddr in_addr;
  socklen_t in_len;
  int infd;
  int local_s = create_and_bind(port);
  int fd = make_socket_non_blocking(local_s);
  if (fd == -1) {
    Log().get(LERR) << "Error creating a socket";
    exit(EXIT_FAILURE);
  }

  s = listen(local_s, SOMAXCONN);
  if(s == -1) {
    Log().get(LERR) << "Listen error";
    exit(EXIT_FAILURE);
  }

  memset(_buffer_recv, 0, MAX_BUFFER_SIZE);
  memset(&ev, 0, sizeof(ev));
  memset(ev_set, 0, sizeof(ev_set));

  in_len = sizeof(in_addr);

  kq = kqueue();

  EV_SET(&ev, local_s, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,	NULL);
	if(kevent(kq, &ev, 1, 0, 0, NULL) == -1) {
    Log().get(LERR) << "Critical kevent error";
  }

  task_struct * tts;
  char * ptr;

  while(1){
    int kv = kevent(kq, 0, 0, ev_set, MAXEVENTS, NULL);

    if(kv < 0) {
      Log().get(LERR) << "Kevent error";
      exit(EXIT_FAILURE);
    }

    for(int i = 0; i < kv; i ++) {
      if(ev_set[i].flags & EV_ERROR || ev_set[i].flags & EV_EOF) {
        if(tasks.find(ev_set[i].ident) != tasks.end()) {
          // Initialize pending completion
          tasks[ev_set[i].ident]->status = false;

          // If the first socket error was initialized in the main thread
          if(!tasks[ev_set[i].ident]->in_round_counter) {
            EV_SET(&ev, ev_set[i].ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            if(kevent(kq, &ev, 1, 0, 0, NULL) == -1) {
              Log().get(LERR) << "Critical kevent error";
            } else {
              Log().get(LDEBUG) << "Successful closed";
            }

            close(ev_set[i].ident);
            clear_buffer(ev_set[i].ident);
          }
        }
      } else if(ev_set[i].ident != local_s) {
        _recv_bytes_count = read(ev_set[i].ident, _buffer_recv, MAX_BUFFER_SIZE);
        
        if(_recv_bytes_count > 0) {
          tts = tasks[ev_set[i].ident];
          ptr = tts->command;

          if(tts->recv_bytes + _recv_bytes_count > MAX_BUFFER_SIZE || !tts->status) {
            if(!tts->status) continue;
            tts->status = false;
            continue;
          }

          // Get data cursor position for addition new data
          int sz_b = tts->recv_bytes;
          memcpy(ptr + sz_b, _buffer_recv, _recv_bytes_count);
          tts->recv_bytes += _recv_bytes_count;

          // Get byte length 
          int sz = get_msg_sz(ptr, tts->recv_bytes);
          if(sz == -1) {  
            memset(_buffer_recv, 0, _recv_bytes_count);
            continue;
          }

          // Get size of prefix(service info) for extracting from total size
          size_t sz_prefix = get_len_prefix(sz);
          
          // Send data with prefix offset
          _round_queue->add(ev_set[i].ident, ptr + sz_prefix, sz);
          tts->in_round_counter ++;

          // Increment current pool counter
          _notify_shed = true;

          // Awaiting some worker
          while(_notify_shed.load(std::memory_order_seq_cst)) {
            _writer_cond.notify_all();
          }

          // Get current byte count
          size_t len = tts->recv_bytes;
          // Reset the receiving counter
          len -= sz_prefix + sz;
          memcpy(ptr, &ptr[sz_prefix + sz], len);
          memset(ptr + len, 0, MAX_BUFFER_SIZE - len);
          memset(_buffer_recv, 0, _recv_bytes_count);
          tts->recv_bytes = len;

        } else if(errno != EWOULDBLOCK && errno != EAGAIN) {
          tasks[ev_set[i].ident]->status = false;
        }
      } else
      if(ev_set[i].ident == local_s) {
        infd = accept(local_s, &in_addr, &in_len);
        if(infd == -1) {
          Log().get(LERR) << "Accept error";
          exit(EXIT_FAILURE);
        }

        int fd = make_socket_non_blocking(infd);
        if (fd == -1) {
          Log().get(LERR) << "Invalid create FD";
          exit(EXIT_FAILURE);
        } else {
          Log().get(LDEBUG) << "New connection with FD #" << fd;
        }

        EV_SET(&ev, infd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        kevent(kq, &ev, 1, NULL, 0, NULL);

        EV_SET(&ev, infd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
        kevent(kq, &ev, 1, NULL, 0, NULL);

        // Init connection struct
        task_struct * ts = new task_struct();
        ts->recv_bytes = 0;
        ts->send_bytes = 0;
        ts->status = true;
        ts->in_round_counter = 0;
        tasks[infd] = ts;
      }
    }
  }

  return 0;
}

// Get length of prefix
int Server::get_len_prefix(int number) {
  size_t prefix_counter = 2;
  while(number > 0) {
    number /= 10;
    prefix_counter ++;
  }
  return prefix_counter;
}

// Get data size in bytes
int Server::get_msg_sz(char buffer[], const size_t SZ) {
  if(buffer[0] != '[') return -1;

  bool flag = false;
  size_t msg_len = 0;
  size_t msg_index = 1;

  // Scanning of request prefix
  while(buffer[msg_index] != '\0' && msg_index <= MAX_LEN_PREFIX && msg_index < SZ) {
    if(buffer[msg_index] == ']') {
      flag = true;
      break;
    }

    // To integer
    msg_len = msg_len * 10 + (buffer[msg_index] - '0');
    msg_index ++;
  }

  // If empty scoupe
  if(msg_index == 1 || !flag) return -1;
  return msg_len;
}
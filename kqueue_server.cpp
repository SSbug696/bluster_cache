#include "kqueue_server.h"

Server::Server(size_t max_pool_sz) {
  cache = new QCache(max_pool_sz);

  initWorkersPool();

  _min_commands_len["s"] = 3;
  _min_commands_len["g"] = 2;
  _min_commands_len["e"] = 2;
  _min_commands_len["d"] = 2;
  _min_commands_len["f"] = 1;
  _min_commands_len["sz"] = 1;

  _assoc_dict_commands["set"]   = SET;
  _assoc_dict_commands["get"]   = GET;
  _assoc_dict_commands["exist"] = EXIST;
  _assoc_dict_commands["del"]   = DEL;
  _assoc_dict_commands["flush"] = FLUSH;
  _assoc_dict_commands["size"]  = SIZE;
}

void Server::data() {
  std::string result;
  std::string command;
  std::string key;
  std::string value;
  size_t COMMAND_ID;
  int type_command = 0;
  std::string tmp_str;

  std::vector<std::string> assets;
  std::vector<std::string> multi_parts;
  std::vector<std::string> string_parts;

  size_t chr_counter = 0;
  short int counter = 0;
  bool is_quote_substring = false;
  bool to_push = false;
  size_t sz = 0;

  char buffer[MAX_BUFFER_SIZE];
  char tmp_buffer[MAX_BUFFER_SIZE];
  std::string command_line;
  size_t expire = 0;
  size_t uid = 0;

  // Unlock main loop
  while(true) {
    std::unique_lock<std::mutex> mlock_rt(_mutex_rw);
    if(_request_queue.empty()) _writer_cond.wait(mlock_rt);

    // Get client ID from
    uid = _request_queue.back();
    _request_queue.pop();

    task_struct * t_ptr = tasks[uid];

    // Reset buffers
    memset(buffer, 0, MAX_BUFFER_SIZE);
    memset(tmp_buffer, 0, MAX_BUFFER_SIZE);

    command_line = t_ptr->command.str();
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
          std::string str(tmp_buffer);
          multi_parts.push_back(str);
          is_quote_substring = false;

          memset(tmp_buffer, 0, sizeof(tmp_buffer));

          counter = 0;
          continue;
        }

        tmp_buffer[counter] = command_line[i];
        counter ++;
      }
    } else multi_parts.push_back(command_line);

    size_t sz_inserts = multi_parts.size();
    counter = 0;
    

    for(size_t k = 0; k < sz_inserts; k ++) {
      // Set default command value
      command_line = multi_parts[k];
      sz = command_line.size();
      
      memset(tmp_buffer, 0, sizeof(tmp_buffer));

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
            // pastream_tmp
          } else {
            tmp_buffer[chr_counter ++] = command_line[i];
          }
        } else {
          if(is_quote_substring == true) {
            tmp_buffer[chr_counter ++] = command_line[i];
          }
          
          if(chr_counter > 0 && is_quote_substring == false) {
            to_push = true;
          }
        }

        // If EOF of request
        if( to_push == true || i >= sz - 1 ) {
          is_quote_substring = false;
          tmp_str = tmp_buffer;

          string_parts.push_back(tmp_str);
          chr_counter = 0;
          
          memset(tmp_buffer, 0, sizeof(tmp_buffer));

          counter ++;
          to_push = false;
        }
      }

      //  Get command ID
      COMMAND_ID = _assoc_dict_commands[ string_parts[0] ];

      //  Command escaping error
      if(is_quote_substring) {
        // Ignore cicle
        counter = 0;
        command = "err";
      }

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

      // Synchronizing threads
      _mutex_cache.lock();

      memset(tmp_buffer, 0, sizeof(tmp_buffer));

      switch(COMMAND_ID) {
        case SET:
          if( assets.size() > 0) {
            std::string exp_label = assets.front();
            if( strspn( exp_label.c_str(), "0123456789" ) == exp_label.size() ) {
              expire = atoi( exp_label.c_str() );
            }
          }
          // If expire is not defined
          if(expire != 0) {
            result = cache->put(key, value, expire);
          } else {
            result = cache->put(key, value);
          }
        break;
      
        case GET:
          result = cache->get(key);
        break;

        case DEL:
          result = cache->del(key);
        break;

        case FLUSH:
          result = cache->flush();
        break;

        case SIZE:
          result = cache->size();
        break;
      
        case EXIST:
          result = cache->exist(key);
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

    command = "";
    key = "";
    value = "";
    type_command = 0;
    tmp_str = "";

    assets.clear();
    multi_parts.clear();
    string_parts.clear();

    chr_counter = 0;
    counter = 0;
    is_quote_substring = false;
    to_push = false;
    sz = 0;

    size_t sz_data = result.size();
    strncpy(t_ptr->send_buffer, result.c_str(), sz_data);

    while(1) {      
      int s = write(uid, &t_ptr->send_buffer[t_ptr->offset],  sz_data - t_ptr->offset);
      if(s == -1) {
        if(errno == EIO) {
          std::cerr << "A low-level I/O error occurred" << std::endl;
          break;
        }

        if(errno == EFAULT) {
          std::cerr << "Buffer is outside your accessible address space" << std::endl;
          break;
        }

        if(errno == EBADF) {
          std::cerr << "Is not valid file descriptor" << std::endl;
          break;
        }

        // Try again
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
          continue;
        } else {
          break;
        }
      } else if(s == 0) {
        break;
      } else {
        t_ptr->offset += s;
        if(t_ptr->offset == sz_data) break;
      }
    }

    t_ptr->terminate = -1;
    result.clear(); 
  }
}

void Server::initWorkersPool() {
  for(size_t i = 0; i < WORKERS_POOL; ++i) 
  _thread_pool.push_back(std::thread(&Server::data, this));

  std::vector<std::thread>::iterator it = _thread_pool.begin();
  for(; it != _thread_pool.end(); it ++ ) {
    it->detach();
  }
}

int Server::make_socket_non_blocking(int sfd) {
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if(flags == -1) {
    std::cerr << "fcntl" << std::endl;
    return -1;
  }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if(s == -1) {
    std::cerr << "fcntl" << std::endl;
    return -1;
  }

  // Not awaiting for closing FD
  struct so_linger {
    int l_onoff;
    int l_linger;
  } linger;

  int reuse = 1;
  linger.l_onoff = 0;
  linger.l_linger = 0;  

  setsockopt(sfd, SOL_SOCKET, SO_LINGER,  &linger, sizeof(linger));
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

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
    std::cerr << "getaddrinfo:"<< gai_strerror(s) << std::endl;
    return -1;
  }

  for(rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
    if (s == 0) {
      /* We managed to bind successfully! */
      break;
    }

    close (sfd);
  }

  if (rp == NULL) {
    std::cerr << "Could not bind" << std::endl;
    return -1;
  }

  freeaddrinfo (result);
  return sfd;
}

void Server::clearBuffer(size_t fd) {
  for(std::map<size_t, task_struct *>::iterator it = tasks.begin(); it != tasks.end(); it ++)
  {
    if((size_t)(it->first) == fd)
    {
      tasks.erase(it);
      break;
    }
  }
}

void Server::rmFD(size_t fd) {
  memset(_buffer_recv, 0, MAX_BUFFER_SIZE);
  if(tasks.count(fd)) {
    tasks[fd]->terminate = -1; 
  } else close(fd);
}

int Server::init(char * port) {
  short int terminate = 0;
  _atomic_lock_nr = false;

  int local_s = create_and_bind(port);
  int fd = make_socket_non_blocking(local_s);
  if (fd == -1) {
    std::runtime_error("Error create fd");
  }
  
  s = listen(local_s, SOMAXCONN);
  if(s == -1) {
    std::runtime_error("Error of listen socket");
    return -1;
  }

  int kq = kqueue();
  EV_SET(&events_set, local_s, EVFILT_READ | EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
  
  if(kevent(kq, &events_set, 1, NULL, 0, NULL) == -1) {
    std::runtime_error("Accept error");
    return -1;
  }

  while(1) {
    terminate = 0;
    // Watch core event
    int nev = kevent(kq, NULL, 0, events_list, 32, NULL);

    if (nev < 1)
      std::clog << "Accept" << std::endl;

    for(int i = 0; i < nev; i ++) { 
      memset(_buffer_recv, 0, MAX_BUFFER_SIZE);

      if(events_list[i].flags & EV_EOF || events_list[i].flags & EV_ERROR) {
        fd = events_list[i].ident;
        EV_SET(&events_set, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

        if(kevent(kq, &events_set, 1, NULL, 0, NULL) == -1)
          std::cerr << "Error of close socket" << std::endl;
        
        rmFD(fd);
        continue;
      } else if(events_list[i].ident == local_s) {
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd;

        in_len = sizeof(in_addr);

        infd = accept(events_list[i].ident, &in_addr, &in_len);
        if (infd == -1) {
          std::cerr << "Accept error" << std::endl;
          continue;
        }
        
        int fd = make_socket_non_blocking(infd);
        if (fd == -1) {
         std::cerr << "Invalid create FD" << std::endl;
        }

        EV_SET(&events_set, infd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        if (kevent(kq, &events_set, 1, NULL, 0, NULL) == -1)
          std::cerr << "Kevent error" << std::endl;
          
      } else  {
        _recv_bytes_count = read(events_list[i].ident, _buffer_recv, MAX_BUFFER_SIZE);
        // If 0 to send SIG of HUP socket
        if(_recv_bytes_count == 0) {
          continue;
        } else if(_recv_bytes_count == -1) {
          if(errno == EWOULDBLOCK || errno == EAGAIN) {
            continue;
          } else {
            // Close descriptor
            rmFD(events_list[i].ident);
            continue;
          }
        } 

        if(_buffer_recv[_recv_bytes_count] == '\0') {
          terminate = 1;
        }

        if(tasks.count(events_list[i].ident)) {
          if(tasks[events_list[i].ident]->terminate == 1) {
            // in processing
            continue;
          } else if(tasks[events_list[i].ident]->terminate == -1) {
            clearBuffer(events_list[i].ident);

            task_struct * ts = new task_struct();
            ts->offset = 0;
            ts->command << _buffer_recv;
            ts->terminate = terminate;
            tasks[events_list[i].ident] = ts;
          } else {
            tasks[events_list[i].ident]->command << _buffer_recv;
            tasks[events_list[i].ident]->terminate = terminate;
          }
        } else {
          task_struct * ts = new task_struct();
          ts->offset = 0;
          ts->command << _buffer_recv;
          ts->terminate = terminate;
          tasks[events_list[i].ident] = ts;
        }

        if (terminate) {
          _request_queue.push(events_list[i].ident);
          _writer_cond.notify_one();
        }

        terminate = 0;
        memset(_buffer_recv, 0, MAX_BUFFER_SIZE);
      }
    }
  }

  return 0;
}
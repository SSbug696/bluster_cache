#include "epoll_server.h"

Server::Server(size_t max_pool_sz) {
  //_buffer_recv = new char(MAX_BUFFER_SIZE);
  memset(&_buffer_recv, 0, sizeof(_buffer_recv));

  cache = new QCache(max_pool_sz);
  intitWorkersPool();

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

  char buffer[1024];
  char tmp_buffer[1024];
  std::string command_line;
  size_t expire = 0;
  task_struct * ptr = 0;


  while(1) {

    if(atomic_lock_nr.load() > 0) {
      atomic_lock_nr.fetch_sub(1, std::memory_order_release);
    } else {
      usleep(100);
      //continue;
    }
    
    // Lock workers
    std::unique_lock<std::mutex> mlock_rt(_mutex_rw);
    // Lock queue container for other threads
    std::unique_lock<std::mutex> mlock_queue(_mutex_queue);
    
    // Reset pointer
    ptr = 0;

    // If queue is empty to unlock mutex
    if(_request_queue.empty()) {
      
      mlock_queue.unlock();
      mlock_rt.unlock();
      continue;
    }

    ptr = _request_queue.front();

    // Remove the last item
    if(ptr > 0) _request_queue.pop();

    // If ptr getting zero to unlock mutex
    if(ptr == 0) {
      mlock_queue.unlock();
      mlock_rt.unlock();
      continue;
    }

    // Unlock mutex
    mlock_queue.unlock();    
    mlock_rt.unlock();

    // Reset buffers
    memset(buffer, 0, sizeof(buffer));
    memset(tmp_buffer, 0, sizeof(tmp_buffer));

    // Get handle client
    int sock_id = ptr->handle_client;

    // Get request text
    sz = ptr->command.size();
    command_line = ptr->command.substr(0, sz);

    // Free memory
    delete ptr;

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
            // pass
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

      //?? std::cout << " OO:: " << string_parts[0] << std::endl;
      //  Get command ID
      COMMAND_ID = _assoc_dict_commands[ string_parts[0] ];

      //  Command escaping error
      if(is_quote_substring) {
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


    // Forwarding to new line
    result.append("\n");

    size_t szs =  result.size();

    memset(buffer, 0, sizeof(buffer));
    strncpy(buffer, result.c_str(), sizeof(buffer));

    buffer[ sizeof(buffer) - 1 ] = 0;

    result.clear(); 
    result = "";
          
    std::unique_lock<std::mutex> mlock_rt_write(_mutex_rw);

    if( 
        (events[sock_id].events & EPOLLERR) ||
        (events[sock_id].events & EPOLLHUP)
      ) {

      close(events[sock_id].data.fd);
    } else {

      while(1) {
        int s = write(events[sock_id].data.fd, buffer, szs);
        
        if(s == -1) {

         if( 
            (events[sock_id].events & EPOLLERR) ||
            (events[sock_id].events & EPOLLHUP)
          ) {
            close(events[sock_id].data.fd);
            break;
          }

          if(errno == EWOULDBLOCK || errno == EAGAIN) {
            continue;
          }

          events[sock_id].events |= EPOLLERR;

        } else if(s == 0) {
          events[sock_id].events |= EPOLLERR;
        }

        break;
      }
    }

    mlock_rt_write.unlock();
  }
}

void Server::intitWorkersPool() {
  //for(size_t i = 0; i < WORKERS_POOL; ++i) 
  //_thread_pool.push_back(std::thread(&Server::data, this));
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
    perror ("fcntl");
    return -1;
  }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if(s == -1) {
    perror ("fcntl");
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

int Server::create_and_bind(char *port) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sfd;

  memset(&hints, 0, sizeof(addrinfo));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;    

  s = getaddrinfo(NULL, port, &hints, &result);
  if(s != 0) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
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
    fprintf (stderr, "Could not bind\n");
    return -1;
  }

  freeaddrinfo (result);
  return sfd;
}

int Server::init(char * port) {

  sfd = create_and_bind(port);
  if (sfd == -1)
    abort ();

  s = make_socket_non_blocking (sfd);
  if (s == -1)
    abort ();

  s = listen (sfd, SOMAXCONN);
  if (s == -1)
    {
      perror ("listen");
      abort ();
    }

  efd = epoll_create1(0);
  if (efd == -1)
    {
      perror ("epoll_create");
      abort ();
    }

  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET | EPOLLOUT;
  s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
  if (s == -1)
    {
      perror ("epoll_ctl");
      abort ();
    }

  /* Buffer where events are returned */
  events = (epoll_event *)calloc(MAXEVENTS, sizeof(event));

  /* The event loop */
  while (1) {
    int n, i;

    // Awaiting some event
    n = epoll_wait(efd, events, MAXEVENTS, -1);
    
    // Iteration by all changed sockets
    for(i = 0; i < n; i++) {

      // HUP socket
      if(
        events[i].events & EPOLLERR ||
        events[i].events & EPOLLHUP ) {

        /* An error has occured on this fd, or the socket is not
           ready for reading (why were we notified then?) */
        fprintf(stderr, "epoll error\n");
        close(events[i].data.fd);
        continue;

      } else if (sfd == events[i].data.fd) {
        /*
          Adding new descriptor
         */
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd;

        in_len = sizeof(in_addr);

        infd = accept(sfd, &in_addr, &in_len);
        if (infd == -1) {
            if ((errno == EAGAIN) ||
                (errno == EWOULDBLOCK)) {
                continue;
        } else  {
            perror("accept");
            continue;
          }
        }
        
        s = make_socket_non_blocking(infd);
        if (s == -1) {
          perror("error of non-block mode");
          continue;
        }

        event.data.fd = infd;
        event.events = EPOLLIN | EPOLLET | EPOLLOUT;
        s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
        
        if(s == -1) {
          perror ("epoll_ctl");
          abort ();
        }

        continue;
      } else {
        // Read data from socket by chunks
        //while(1) {
        _recv_bytes_count = read(events[i].data.fd, _buffer_recv, sizeof(_buffer_recv));
               
        // If 0 to send SIG of HUP socket
        if(_recv_bytes_count == 0) {
          events[i].events |= EPOLLHUP;
          continue;

        } else if(_recv_bytes_count == -1) {
          
          // If socket can't be read
          if(errno != EAGAIN && errno != EWOULDBLOCK) 
            events[i].events |= EPOLLERR;
          
          continue;
        }
        
        std::string str(_buffer_recv);

        task_struct * ts = new task_struct();
        ts->command = _buffer_recv;
        ts->handle_client = i;

        memset(_buffer_recv, 0, sizeof(_buffer_recv));

        atomic_lock_nr.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> mlock_queue(_mutex_queue);
        
        _request_queue.push(ts);

        mlock_queue.unlock();      
      }
    }
  }

  free(events);
  close(sfd);

  sleep(1000);
  return EXIT_SUCCESS;
}
#include "server.h"

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
    Log().get(LERR) << "Getaddrinfo:" << gai_strerror(s);
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
      // Managed to bind successfully
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

  // Pointer to request struct
  task_struct * t_ptr;

  bool is_quote_substring = false;
  bool is_pushed = false;
  char buffer_source[MAX_BUFFER_SIZE];

  int chunk_offset = 0;
  int is_flag = 1;
  int wc = 0;

  std::unique_lock<std::mutex> mlock_rt(_mutex_rw);

  memset(buffer_source, 0, MAX_BUFFER_SIZE);

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
      for(int i = 1; i < sz; i ++) {
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

      for(int i = 0; i < sz; i ++) {
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

    // While until buffer isn't flushed and don't exist error
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

    sz = 0;
    // Request is performed. Decrement of request
    t_ptr->in_round_counter --;
    memset(buffer_source, 0,  chunk_offset);
  }
}

void Server::init_workers_pool() {
  for(size_t i = 0; i < WORKERS_POOL; ++i) {
    _thread_pool.push_back(std::thread(&Server::do_task, this));
  }

  for_each(_thread_pool.end(), _thread_pool.end(), [](std::thread &t){ t.detach(); });
}

int Server::make_socket_non_blocking(int sfd) {
  int flags;

  flags = fcntl(sfd, F_GETFL, 0);
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

  setsockopt(sfd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable_flag, sizeof(enable_flag));
  setsockopt(sfd, SOL_SOCKET, TCP_NODELAY, &enable_flag, sizeof(enable_flag));

  return 0;
}

void Server::clear_buffer(size_t fd) {
  delete tasks[fd];
  tasks.erase(fd);
}

void Server::rm_fd(size_t fd) {
  memset(_buffer_recv, 0, MAX_BUFFER_SIZE);
  close(fd);
}
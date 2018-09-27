#include "server.h"

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

int Server::init(size_t port) {
  struct sockaddr in_addr;
  socklen_t in_len;
  task_struct * tts;
  char * ptr;

  local_s = create_and_bind(port);
  if(local_s == -1) {
    Log().get(LERR) << "Socket binding error";
    exit(EXIT_FAILURE);
  }

  s = make_socket_non_blocking(local_s);
  if(s == -1) {
    Log().get(LERR) << "Error of non-blocking mode";
    exit(EXIT_FAILURE);
  }

  s = listen(local_s, SOMAXCONN);
  if(s == -1) {
    Log().get(LERR) << "Error of socket listening";
    exit(EXIT_FAILURE);
  }

  efd = epoll_create1(0);
  if(efd == -1) {
    Log().get(LERR) << "Critical error in initialization EPOLL";
    exit(EXIT_FAILURE);
  }

  event.data.fd = local_s;
  event.events = EPOLLIN | EPOLLHUP;
  s = epoll_ctl(efd, EPOLL_CTL_ADD, local_s, &event);

  if(s == -1) {
    Log().get(LERR) << "CTL critical error";
  }

  while(1) {
    int n = epoll_wait(efd, events, MAXEVENTS, -1);
    for(int i = 0; i < n; i++) {
      if(
        events[i].events & EPOLLERR ||
        events[i].events & EPOLLRDHUP ||
        events[i].events & EPOLLHUP
      ) {
        if(tasks.find(events[i].data.fd) != tasks.end()) {
          // Initialize pending completion
          tasks[events[i].data.fd]->status = false;

          // If the first socket error was initialized in the main thread
          if(!tasks[events[i].data.fd]->in_round_counter) {
            if(epoll_ctl(efd, EPOLL_CTL_DEL, events[i].data.fd, &event)  == -1) {
              Log().get(LERR) << "Critical epoll error";
            }

            rm_fd(events[i].data.fd);
            clear_buffer(events[i].data.fd);
          }
        }
      } else if (local_s == events[i].data.fd) {
        int nfd = accept(local_s, &in_addr, &in_len);
        if(nfd == -1) {
          Log().get(LERR) << "Accept error";
        }

        int fd = make_socket_non_blocking(nfd);
        if (fd == -1) {
          Log().get(LERR) << "Error of non-blocking mode";
          close(nfd);
          continue;
        } else {
          Log().get(LDEBUG) << "New connection with FD #" << fd;
        }

        event.events = EPOLLRDHUP | EPOLLIN | EPOLLOUT | EPOLLERR;
        event.data.fd = nfd;

        s = epoll_ctl(efd, EPOLL_CTL_ADD, nfd, &event);
        if(s  == -1) {
          Log().get(LERR) << "CTL critical error";
          close(nfd);
          continue;
        }

        // Init connection struct
        task_struct * ts = new task_struct();
        ts->recv_bytes = 0;
        ts->send_bytes = 0;
        ts->status = true;
        ts->in_round_counter = 0;
        tasks[nfd] = ts;
      } else if(events[i].events & EPOLLIN) {
        _recv_bytes_count = read(events[i].data.fd, _buffer_recv, MAX_BUFFER_SIZE);

        if(_recv_bytes_count > 0) {
          tts = tasks[events[i].data.fd];
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
          _round_queue->add(events[i].data.fd, ptr + sz_prefix, sz);

          // Increment current pool counter
          tts->in_round_counter ++;

          // Awaiting some worker
          _notify_shed = true;
          while(_notify_shed) {
            _writer_cond.notify_one();
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
          events[i].events |= EPOLLERR;
          tasks[events[i].data.fd]->status = false;
        }
      }
    }
  }

  return 0;
}

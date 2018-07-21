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
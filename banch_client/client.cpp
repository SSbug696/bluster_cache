#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <errno.h>
#include <sys/wait.h>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include <mutex>
#include <atomic>
#include <vector>

#define BUFFER_LIMIT 1024
#define REQ_UPSTAT_INTERVAL 10000

using namespace std;

std::atomic<int> QC {0};
static int COUNT_THREADS;

// To pack data and and fill buffer some ops
void fill_buffer(char * buffer, int sz, const char * str) {
  stringstream ss;
  string str_ops(str);
  ss.str("");    
  ss << "[" << (str_ops.size()) << "]" << str;
  str_ops = ss.str();
  str_ops.copy(buffer, str_ops.size(), 0);
}

// Generator of batch list / SET
std::string rq_generator(bool is_batch, size_t len, size_t key_len, size_t val_len) {
  std::stringstream ss;
  const size_t SC = 25;
  char abc[SC];

  srand( time(NULL) );

  for(int i = 97; i <= 122; i ++) {
    abc[i - 97] = (char)i;
  }

  if(is_batch)
    ss << "[";

  for(int i = 0; i < len; i ++) {
    ss << "set ";
    for(int k = 0; k < key_len; k ++) ss << abc[rand() % SC];
    ss << " ";
    for(int k = 0; k < val_len; k ++) ss << abc[rand() % SC];
    if(i + 1 < len) {
      ss << ",";
    }
  }

  if(is_batch)
    ss << "]";

  return std::move(ss.str());
}

void batch_test(int s, size_t rq, size_t len, size_t key_len, size_t val_len, bool is_main) {
  const size_t SZ = (len * (key_len + val_len + 5)) + 2;
  char buffer[SZ];
  char buffer_recv[SZ];

  memset(buffer, 0, sizeof(buffer));

  std::string batch(rq_generator(true, len, key_len, val_len));

  fill_buffer(buffer, batch.size(), batch.c_str());


  if(is_main) {
    std::cout << "Total request: " << (rq * len)
              << "\nMode: pipeline"
              << "\nPayload per request: " << (key_len + val_len)
              << "\nCount messages per request: " << len << "\n";
  }

  auto start = chrono::steady_clock::now();
  float trange;

  size_t request_lim = REQ_UPSTAT_INTERVAL;
  if(request_lim > QC) {
    request_lim = 1;
  }

  int i = 0;
  while(QC -- > 0) {
    write(s, buffer, strlen(buffer));
    read(s, buffer_recv, SZ);

    if(is_main) {
      if(i % request_lim == 0) {
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        trange = chrono::duration<float, milli> (diff).count() / 1000;
        printf("%c[2K", 27);
        printf("RPS: %i\r", int((((rq - QC) * len) / trange)));
        fflush(stdout);
      }
    }

    i ++;
  }

  if(is_main) {
    printf("%c[2K", 27);
    std::cout << "--- EXECUTED ---"
              << "\nElapsed time: " << trange << " ms."
              << "\nAverage RPS:" << int((rq * len) / trange) << "\n";
  }
}

void mono_test(int s, size_t rq, size_t key_len, size_t val_len, bool is_main) {
const size_t SZ = key_len + val_len  + 5;
  char buffer[SZ];
  char buffer_recv[SZ];
  float trange;
  size_t request_lim = REQ_UPSTAT_INTERVAL;

  memset(buffer, 0, sizeof(buffer));

  std::string batch(rq_generator(false, 1, key_len, val_len));

  fill_buffer(buffer, batch.size(), batch.c_str());

  if(is_main) {
    std::cout << "Total request: " << rq
              << "\nMode: by request"
              << "\nPayload per request: "
              << (key_len + val_len) << " bytes\n";
  }

  if(request_lim > QC) {
    request_lim = 1;
  }

  auto start = chrono::steady_clock::now();

  int i = 0;

  while(QC -- > 0) {
    write(s, buffer, strlen(buffer));
    read(s, buffer_recv, SZ);

    if(is_main) {
      if(i % request_lim == 0) {
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        trange = chrono::duration<float, milli> (diff).count() / 1000;
        printf("%c[2K", 27);
        printf("RPS: %i\r", int(((rq * COUNT_THREADS) - QC) / trange));
        fflush(stdout);
      }
    }

    i ++;
  }

  if(is_main) {
    printf("%c[2K", 27);
    std::cout << "--- EXECUTED ---"
              << "\nElapsed time: " << trange << " ms."
              << "\nAverage RPS:" << int(rq / trange) << "\n";
  }
}


void run(int argc, char const *argv[], bool is_main) {
  struct sockaddr_in serv_addr;
  int sock = 0;
  int REQ_LIMIT = atoi(argv[3]);
  int PORT = atoi(argv[2]);

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error("Socket creation error");
  }

  if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) == -1) {
    throw std::runtime_error("Invalid address/address not supported");
  }

  if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
    throw std::runtime_error("Connection Failed");
  }

  if(strcmp(argv[5], "batch") == 0) {
    batch_test(sock, REQ_LIMIT, atoi(argv[6]),  atoi(argv[7]),  atoi(argv[8]), is_main);
  } else 
  if(strcmp(argv[5], "mono") == 0) {
    mono_test(sock, REQ_LIMIT, atoi(argv[6]),  atoi(argv[7]), is_main);
  } else {
    throw std::runtime_error("Unknown test mode");
  }
}

int main(int argc, char const *argv[]) {
  for(int i = 6; i < argc; i ++) {
    if(atoi(argv[i]) <= 0)
      throw std::runtime_error("Zero parameters isn't suppoerted");
  }

  std::vector<std::thread> vthreads;
  COUNT_THREADS = atoi(argv[4]);
  QC = atoi(argv[3]);

  for(int i = 0; i < atoi(argv[4]); i ++) {
    vthreads.emplace_back(run, argc, argv, (i == 0 ? true : false ));
  }

  for(auto &v: vthreads)
    v.join();

  return 0;
}

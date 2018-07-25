#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <ctime>
#include <string.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>

#define BUFFER_LIMIT 1024
#define REQ_UPSTAT_INTERVAL 10000

using namespace std;

std::atomic<int> QC {0};
static int COUNT_THREADS;

// Get length of prefix
int get_len_prefix(size_t number) {
  size_t prefix_counter = 2;
  while(number > 0) {
    number /= 10;
    prefix_counter ++;
  }

  return prefix_counter;
}

// To pack data and and fill buffer some ops
int fill_buffer(char * buffer, int sz, const char * str) {
  stringstream ss;
  string str_ops(str);
  ss.str("");
  ss << "[" << (str_ops.size()) << "]" << str;
  str_ops = ss.str();
  str_ops.copy(buffer, str_ops.size(), 0);

  // Return string size with prefix
  return get_len_prefix(str_ops.size()) + 2;
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
  size_t request_lim = REQ_UPSTAT_INTERVAL;
  const size_t SZ = 0xff * 0xff;
  char buffer[SZ];
  char buffer_recv[SZ];
  float trange;
  int i = 0;

  std::string batch(rq_generator(true, len, key_len, val_len));
  const size_t prefix_len = fill_buffer(buffer, batch.size(), batch.c_str());
  buffer[prefix_len + batch.size()] = '\0';

  if(is_main) {
    std::cout << "Total requests: " << (rq * len)
              << "\nMode: pipeline"
              << "\nPayload per request: " << (key_len + val_len) << " bytes"
              << "\nCount messages per request: " << len << "\n";
  }

  if(request_lim > rq) {
    request_lim = 1;
  }

  auto start = chrono::steady_clock::now();
  size_t str_len = strlen(buffer);

  while(QC -- >= 0) {
    write(s, buffer, str_len);
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
              << "\nAverage RPS: " << int((rq * len) / trange) << "\n\n";
  }
}

void mono_test(int s, size_t rq, size_t key_len, size_t val_len, bool is_main) {
  size_t request_lim = REQ_UPSTAT_INTERVAL;
  const size_t SZ = 0xff * 0xff;
  char buffer[SZ];
  char buffer_recv[SZ];
  float trange;
  int i = 0;

  std::string batch(rq_generator(false, 1, key_len, val_len));
  const size_t prefix_len = fill_buffer(buffer, batch.size(), batch.c_str());
  buffer[prefix_len + batch.size()] = '\0';

  if(is_main) {
    std::cout << "Total requests: " << rq
              << "\nMode: by request"
              << "\nPayload per request: "
              << (key_len + val_len) << " bytes\n";
  }

  if(request_lim > rq) {
    request_lim = 1;
  }

  auto start = chrono::steady_clock::now();
  size_t str_len = strlen(buffer);

  while(QC -- >= 0) {
    write(s, buffer, str_len);
    read(s, buffer_recv, SZ);

    if(is_main) {
      if(i % request_lim == 0) {
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        trange = chrono::duration<float, milli> (diff).count() / 1000;
        printf("%c[2K", 27);
        printf("RPS: %i\r", int((rq - QC) / trange));
        fflush(stdout);
      }
    }

    i ++;
  }

  if(is_main) {
    printf("%c[2K", 27);
    std::cout << "--- EXECUTED ---"
              << "\nElapsed time: " << trange << " ms."
              << "\nAverage RPS: " << int((rq - QC) / trange) << "\n\n";
  }
}

void run(std::map<std::string, std::string> argv, bool is_main) {
  struct sockaddr_in serv_addr;
  int sock = 0;
  int PORT = std::stoi(argv["-p"]);

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error("Socket creation error");
  }

  if(inet_pton(AF_INET, argv["-h"].c_str(), &serv_addr.sin_addr) == -1) {
    throw std::runtime_error("Invalid address/address not supported");
  }

  if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
    throw std::runtime_error("Connection Failed");
  }

  if(argv["-m"] == "batch") {
    batch_test(sock, std::stoi(argv["-r"]), std::stoi(argv["-bsz"]), std::stoi(argv["-ksz"]),  std::stoi(argv["-vsz"]), is_main);
  } else
  if(argv["-m"] == "mono") {
    mono_test(sock, std::stoi(argv["-r"]), std::stoi(argv["-ksz"]),  std::stoi(argv["-vsz"]), is_main);
  } else {
    throw std::runtime_error("Unknown test mode");
  }
}

int main(int argc, char const *argv[]) {
  std::vector<std::string> params;
  std::vector<std::thread> vthreads;
  std::map<std::string, std::string> keywords;
  std::stringstream cmd_line;

  std::map <std::string, size_t> enable_cmd {
    // host
    std::make_pair("-h", 0),
    // port
    std::make_pair("-p", 0),
    // parallels clients
    std::make_pair("-c", 0),
    // mode(string)
    std::make_pair("-m", 0),
    // total request
    std::make_pair("-r", 0),
    std::make_pair("-ksz", 0),
    std::make_pair("-vsz", 0),
    // batch size
    std::make_pair("-bsz", 0)
  };

  for(int i = 1; i < argc; i ++) {
    cmd_line << " " << argv[i];
  }

  const size_t SZ = cmd_line.str().size();
  char buffer[SZ];
  cmd_line.str().copy(buffer, SZ);
  cmd_line.str("");

  for(int i = 0; i < SZ; i ++) {
      if(buffer[i] != ' ') {
          while(buffer[i] != ' ' && i < SZ) {
              cmd_line << buffer[i];
              i ++;
          }

          params.push_back(cmd_line.str());
          cmd_line.str("");
      }
  }

  if(params.size() % 2) {
      std::cerr << "Invalid count of params. Need key-value consistency" << std::endl;
  }

  for(int i = 0; i < params.size() - 1; i += 2 ) {
      if(enable_cmd.count(params[i])) {
          keywords[params[i]] = params[i+1];
      } else {
          std::cerr << "Unknown command " << params[i] << std::endl;
          return 1;
      }
  }

  if(!keywords.count("-h")) {
    keywords["-h"] = "127.0.0.1";
  }

  if(!keywords.count("-p")) {
    std::cerr << "Invalid parameter -p" << std::endl;
    return 1;
  }

  if(!keywords.count("-ksz")) {
    keywords["-ksz"] = "3";
  } else if(std::stoi(keywords["-ksz"]) <= 0) {
    std::cerr << "Too small size parameter -ksz" << std::endl;
    return 1;
  }


  if(!keywords.count("-vsz")) {
    keywords["-vsz"] = "3";
  } else if(std::stoi(keywords["-vsz"]) <= 0) {
    std::cerr << "Too small size parameter -vsz" << std::endl;
    return 1;
  }

  if(!keywords.count("-m")) {
    keywords["-m"] = "mono";
  } else if(keywords["-m"] != "batch" && keywords["-m"] != "mono") {
    std::cerr << "Invalid parameter -m. Only available batch/mono modes" << std::endl;
    return 1;
  }

  if(!keywords.count("-c")) {
    keywords["-c"] = "2";
  } else if(std::stoi(keywords["-c"]) <= 0) {
    std::cerr << "Too small size parameter -c" << std::endl;
    return 1;
  }

  if(!keywords.count("-bsz")) {
    keywords["-bsz"] = "20";
  } else if(std::stoi(keywords["-bsz"]) <= 0) {
    std::cerr << "Too small size parameter -bsz" << std::endl;
    return 1;
  }

  if(!keywords.count("-r")) {
    keywords["-r"] = "100000";
  } else if(std::stoi(keywords["-r"]) < REQ_UPSTAT_INTERVAL) {
    std::cerr << "Too small size parameter -r. Minimum 10k" << std::endl;
    return 1;
  }

  if(std::stoi(keywords["-c"]) >= SOMAXCONN || std::stoi(keywords["-c"]) <= 0) {
    std::cerr << "Too many simultaneous connections. Check the SOMAXCONN option in your OS" << std::endl;
    return 1;
  }

  std::cout << "\nTest client is running on " << keywords["-h"] << ":" <<keywords["-p"]
            << "\nParallel clients: " << keywords["-c"] << std::endl;

  COUNT_THREADS = std::stoi(keywords["-c"]);
  QC = std::stoi(keywords["-r"]);

  for(int i = 0; i < COUNT_THREADS; i ++) {
    vthreads.emplace_back(run, keywords, (i == 0 ? true : false ));
  }

  for(auto &v: vthreads)
    v.join();

  return 0;
}

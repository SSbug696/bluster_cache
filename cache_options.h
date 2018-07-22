// Maximum buffer size for each client(max. length of command in bytes)
#define MAX_BUFFER_SIZE  4096
// Maximum number of events to listen to
#define MAXEVENTS  1000
// Max. count of thread in workers pool
#define WORKERS_POOL 3
// Max. prefix length for each message(in characters)
#define MAX_LEN_PREFIX 9

// Enable/disable output to file
#define FILE_LOG 0
// For console
#define CONSOLE_LOG 1

// LDEBUG(0), LINFO(1), LWARN(2), LERR(3)
#define LOG_TYPE 0
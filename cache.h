#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include "unistd.h"
#include <set>
#include <queue>
#include <unordered_map>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

class QCache {
  public:
    /*
      Override methods PUT for expire parameter
     */
    std::string put(std::string, std::string, size_t);
    std::string put(std::string, std::string);
    std::string get(std::string);
    std::string del(std::string);
    std::string flush();
    std::string size();
    std::string exist(std::string);
    std::atomic<size_t> atomic_lock_nr;

    QCache(size_t);

  private:
    // ID's of action
    enum  actions { WRITE = 1, REMOVE_ONCE, REMOVE_ALL, GET_ALL };

    // Max size of pool records and current sequence
    int _limit, _current_sz;
    size_t _sleep_active;
    
    // Workers pool
    std::vector<std::thread> workers_pool;

    // Mutex for locking resources
    std::mutex _mutex_crl;
    std::mutex _mutex_w;
    std::mutex _mutex_rw;

    // Double-linked list for quick displacement
    struct List {
      List * prev;
      List * next;
      std::string val;
      std::string key;
      
      size_t time_expire_label;
      
      List() {
        time_expire_label = -1;
        prev = 0;
        next = 0;
        val = "";
        key = "";
      }

      ~List() {/*pass*/}
    };

    // Record stuct
    struct NodeAction {
      std::string key;
      std::string val;
      size_t expire;
      size_t type_action;
    };

    // Struct of task
    struct SheduleStruct {
      std::string key;
      size_t action;
    };

    // Hash in mem persisting
    std::unordered_map<std::string, List *> _kv_map;
    // Hash buffer for quiqly access
    std::unordered_map<std::string, std::string> _kv_tmp;
    // Hash for control expire time
    std::map<size_t, std::unordered_map<std::string, size_t>> _expires_leave;
    // Queue for records structs
    std::queue<NodeAction *> _sheduler_buffer;

    // Hash for old records in common pool
    std::map<std::string, size_t> _map_locked;

    // The first and last node
    List * _first;
    List * _last;

    char buffer_wraped_string[1024];
    void _write(std::string, std::string, size_t);

    // Method with mutex trigger for access to records
    std::string _lockedKeyAccess(std::string, size_t);
    
    void _taskDelegateToCommonOperations();
    void _taskDelegateToSheduler();
    std::string _strWrap(std::string);
    std::string _strWrap(std::string, size_t);

    void _runSheduleWorkers();
    void _vacuumCache(std::string);
    void _expireLimitLeave();
    void _cacheSheduleResolve();
};
#include <atomic>
#include <string>
#include <mutex>

class RRList {
  private:
    struct Node {
      Node *r;
      Node *l;
      int fd;
      char buffer[2048];
      size_t ln;
    };

  std::mutex _m_lock;
  int _count_nodes;
  Node *_head, *_it_ptr;
  
  public:
    RRList(): _head(nullptr), _count_nodes(0) {};
    ~RRList();
    
    void add(int, char buff[], size_t);
    void rm(int);
    void rm_all(int);
    int get_sz_pool();
    void print();
    inline void lock();
    inline void unlock();
    int next(int);
    int next_slice(std::string &);
    int get_length();
};
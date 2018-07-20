#include "qcache.h"

QCache::QCache(size_t size_queue) {
  _limit      = size_queue;
  _current_sz = 0;
  _first      = 0;
  _last       = 0;

  run_workers_shedule();
  _up_ttl = 0;
  std::unordered_map<std::string, size_t> map_expire_disable;
  // Set map for old node. This hash can't be removed
  _expires_leave[1] = map_expire_disable;    
}

void QCache::ttl_shedule() {}

void QCache::ops_sheduler() {
  while(1) {
    check_expire();
    usleep(TRATE_CHECK_BF);
    ops_resolve();
  }
}

// Running watcher of records expires
void QCache::run_workers_shedule() {
  workers_pool.push_back( std::thread(&QCache::ops_sheduler, this) );
  for_each(workers_pool.begin(), workers_pool.end(), [](std::thread & _n) {
    _n.detach();
  });
}

void QCache::push(NodeAction * ptr) {
  std::unique_lock<std::mutex> mlock(_mutex_crl);
  _sheduler_buffer.push(ptr);
  mlock.unlock();
}

// Push task to queue
std::string QCache::put(std::string && key, std::string && val) {
  NodeAction * ptr = new NodeAction();
  ptr->key = key;
  ptr->val = val;
  ptr->expire = 0;
  ptr->type_action = WRITE;
  _kv_tmp[key] = val;  
  push(ptr);
  return ONE;
}

std::string QCache::del(std::string && key) {
  NodeAction * ptr = new NodeAction();
  ptr->key = key;
  ptr->type_action = REMOVE_ONCE;

  push(ptr);

  if( _kv_map.count(key) ) {
    return ONE;
  } else return ZERO;
}

std::string QCache::flush() {
  NodeAction * ptr = new NodeAction();
  ptr->type_action = REMOVE_ALL;
  push(ptr);
  return ONE;
}

// Push task to queue
std::string QCache::put(std::string && key, std::string && val, size_t expire) {
  NodeAction * ptr = new NodeAction();
  ptr->key = key;
  ptr->val = val;
  ptr->expire = expire;
  ptr->type_action = WRITE;

  _kv_tmp[key] = val;
  push(ptr);

  return ONE;
}

std::string QCache::size() {
  return std::to_string( _kv_map.size() );
}

std::string QCache::exist(std::string && key) {
  std::string result = get(std::forward<std::string>( key ));
  
  if(result != "(null)") {
    return ONE;
  } else return ZERO;
}

std::string QCache::get(std::string && key) {
  std::unique_lock<std::mutex> mlock(_mutex_crl);
  std::string val = to_lock_key(key, 1);
  mlock.unlock();
  return val; 
}

// Set to delete queue for old node
void QCache::check_expire() {
  if(_expires_leave.empty()) {
    return;
  }

  std::unique_lock<std::mutex> mlock_crl(_mutex_crl);
  size_t time_stamp = time(NULL);

  it = _expires_leave.begin();   
  // Node with current timestamp is found
  for(;it != _expires_leave.end();) {
    // Iterator for each node 
    v_iter = it->second.begin();

    for(;v_iter != it->second.end(); ) {
      // Remove duplication in the buffer, counter for blocked nodes
      if(v_iter->second <= time_stamp && v_iter->second > 0 ) { 
        // Guard for pair set/get       
        do_vacuum_cache(v_iter->first);

        v_iter->second = 0;

      } else if( v_iter->second == 0 ) {
        _map_locked[v_iter->first] = it->first;
      }

      v_iter ++;
    }

    it++;
  }

  it_lock = _map_locked.begin();
  for(;it_lock != _map_locked.end(); it_lock ++) {
    if(_expires_leave.count(it_lock->second)) {

      if(_expires_leave[it_lock->second].count(it_lock->first)) {
        _expires_leave[it_lock->second].erase(it_lock->first);
      }

      if(it_lock->second != 1 && _expires_leave[it_lock->second].empty()) {
        _expires_leave.erase(it_lock->second);
      }
    }
  }

  _map_locked.clear();
  mlock_crl.unlock();
}

// Set values in queue from main input stream
void QCache::ops_resolve() {
  NodeAction * ptr = 0;
  int record_produce = MAX_REQ_PROCESSING;
  // Sync with pool records
  while( !_sheduler_buffer.empty()  && record_produce --) {
    ptr = _sheduler_buffer.front();

    if(ptr == 0) continue;
    
    std::unique_lock<std::mutex> mlock_crl(_mutex_crl);

    switch(ptr->type_action) {
      case WRITE:
        write(std::move(ptr->key), std::move(ptr->val), ptr->expire);
      break;

      case REMOVE_ONCE:
        do_vacuum_cache(ptr->key);
      break;

      case REMOVE_ALL:
        _expires_leave.clear();
        _kv_map.clear();
        _kv_tmp.clear();

        std::unordered_map<std::string, size_t> map_expire_disable;
        // Set map for old node. This hash can't be removed
        _expires_leave[1] = map_expire_disable; 

        _last = 0;
        _first = 0;
      break;
    }
    
    mlock_crl.unlock(); 
    _sheduler_buffer.pop();
  }
}

std::string QCache::to_lock_key(std::string key, size_t type) {
  if(type == 1) {   
    if(_kv_map.count(key)) {
      if(_kv_tmp.count(key)) {
        _kv_tmp.erase(key);
      } 

      return _kv_map[key]->val;  
    
    } else if(_kv_tmp.count(key)) {
    
      return _kv_tmp[key];
    }

  } else if(type == 2) {

    if(_kv_tmp.count(key)) {
      _kv_tmp.erase(key);
    }

    if(_kv_map.count(key)) {
      _kv_map.erase(key);
    }
  }

  return "(null)"; 
}

// Clear node by key
void QCache::do_vacuum_cache(std::string key) {
  // If current key isn't exist
  if( _kv_map.count(key) == 0 ) {
    return;
  }
  
  List * target_node = _kv_map[key];

  if(target_node->prev == 0 && target_node->next == 0) {

    _last = 0;
    _first = 0;

  } else if(target_node->prev != 0) {
    // If not the first item
    List * l_node  = target_node->prev;
    List * r_node  = target_node->next;
    
    // If last node
    if(r_node == 0) {
      // If last item (last node have the tail is value as 0)
      // ** Target_node is last node
      // Offset to left tail node
      if(_last->prev) {
        _last = _last->prev;
        _last->next = 0;
      }

    } else if(l_node && r_node) {
      // If current node isn't the first(left and right nodes is exist)
      l_node->next = r_node;
      r_node->prev = l_node;
    } 
  
  } else {
    // If the node isn't one rebalance list
    if(_first->next != 0) {
      _first = _first->next;
      _first->prev = 0;
    }
  }

  to_lock_key(key, 2); 
}

// Write data to cache
void QCache::write(std::string && key, std::string && val, size_t expire) {
  // Get current timestamp
  size_t time_stamp = time(NULL);
  // Get time of last tick for record
  size_t t_leave = time_stamp + expire;

  //  The hash of the lifetime records
  if( _expires_leave.count(t_leave) == 0) {
    std::unordered_map<std::string, size_t> map_expire_disable;
    _expires_leave[t_leave] = map_expire_disable;   
  }

  List * target_node = 0;

  // Inserting the first element
  if( _last == 0 || _first == 0) {

    List * target_node = new List();
    target_node->key = key;
    target_node->val = val;
    //target_node->time_expire_label = 0;
    _last = target_node;
    _first = target_node;
    _kv_map[key] = target_node;

  } else {
    // If node is founded and have other date else break
    if(_kv_map.count(key) > 0) {
      target_node = _kv_map[key];
    }

    // If added node with uniq key
    if(target_node == 0) {
      // Set new item 
      target_node = new List();
      target_node->next = _first;
      target_node->val = val;
      target_node->key = key;
      target_node->prev = 0;

      _kv_map[key] = target_node;

      // Set as _first node
      _first->prev = target_node;
      _first = target_node;
      
      // Link to _last node
      if(_kv_map.size() == 1) {
        _last = _first->next;
        _last->next = 0;
        _last->prev = _first;
      }

    } else if(target_node != 0) {

      //  Checking exist curent key in garbage buffer
      if( _expires_leave.count( target_node->time_expire_label ) ) {
        if( _expires_leave[target_node->time_expire_label].count(target_node->key) ) {
          // Erase key from flush buffer(set new value of time expire)
          _expires_leave[target_node->time_expire_label].erase(target_node->key);
        }
      } 

      //  Update value of record
      target_node->val = val;
    
      /* If key is exist and cache is overload */
      if(_kv_map.size() > 1) {

        List * l_node = 0;
        List * r_node = 0;

        // If current item isn't first(left node is exist)
        if(target_node->prev != 0) {
          l_node  = target_node->prev;
          r_node  = target_node->next;

          // If last item (last node have the tail is value as 0)
          // ** Target_node is last node
          // Offset to left tail node
          if(r_node == 0) {
            // Repeat block code.***
            _first->prev = target_node;
            target_node->next = _first;
            target_node->prev = 0; 
            _first = target_node;

          } else if(l_node && r_node) {
            // If current node isn't the first(left and right nodes is exist)
            l_node->next = r_node;
            r_node->prev = l_node;

            _first->prev = target_node;
            target_node->next = _first;
            target_node->prev = 0; 
            _first = target_node;
          }
        }
      }
    }
  }

  // Pointer to tail node
  target_node = _last;
  while( _kv_map.size() > _limit && target_node) { 
    // Removing the сache tail
    do_vacuum_cache(target_node->key);
    target_node = target_node->prev;
  }

  // Set time expire
  if(expire != 0) { 
    _kv_map[key]->time_expire_label = t_leave;
    _expires_leave[t_leave][key] = t_leave;
  } else _kv_map[key]->time_expire_label = t_leave;
}
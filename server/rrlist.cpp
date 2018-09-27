#include "rrlist.h"
#include <iostream>

RRList::~RRList() {
  _m_lock.lock();
  if(_count_nodes) {
    while(_head->r != nullptr) {
      _it_ptr = _head;
      _head->l->r = nullptr;
      _head = _head->r;
      delete _it_ptr;
    }
    delete _head;
  }

  _count_nodes = 0;
  _m_lock.unlock();
}

int RRList::get_length() {
  return _count_nodes;
}

// Round-robin iterator
int RRList::next(int direction) {
  _m_lock.lock();
  int fd;

  if(_count_nodes != 0) {
    fd = _head->fd;

    // If exist right node (-1, 0, 1) - left, don't offset, right
    if(_count_nodes > 1) {
      if(direction > 0) {
        _head = _head->r;
      } else if(direction < 0) {
        _head = _head->l;
      }
    }
  } else fd = -1;

  _m_lock.unlock();
  return fd;
}

int RRList::next_slice(std::string & buff) {
  int fd = -1;
  _m_lock.lock();
  if(_count_nodes > 0) {
    buff = _head->buffer;
    fd = _head->fd;

    if(_count_nodes > 1) {
      Node * ptr;
      _head->l->r = _head->r;
      _head->r->l = _head->l;
      ptr = _head->r;
      delete _head;
      _head = ptr;
    } else {
      delete _head;
      _head = nullptr;
    }

    _count_nodes --;
  }
  _m_lock.unlock();
  return fd;
}

// Round-robin iterator
int RRList::get_sz_pool() {
  _m_lock.lock();
  int n = _count_nodes;
  _m_lock.unlock();

  return n;
}

void RRList::add(int fd, char buffer[], size_t len) {
  Node * n = new Node();
  n->fd = fd;
  n->r = nullptr;
  n->l = nullptr;
  memcpy(n->buffer, buffer, len);
  n->ln = len;

  _m_lock.lock();
  if(_head == nullptr) {
    _head = n;
    n->l = n;
    n->r = n;
  } else {
    _head->r->l = n;
    n->r = _head->r;
    n->l = _head;
    _head->r = n;
  }

  _count_nodes ++;
  _m_lock.unlock();
}

void RRList::rm(int fd) {
  _m_lock.lock();
  int rm_count = 0;

  if(_head != nullptr) {
    for(int i = 0; i < _count_nodes; i ++) {
      if(_head->fd == fd) {
        if(_count_nodes - rm_count > 1) {
          _it_ptr = _head->r;

          _head->r->l = _head->l;
          _head->l->r = _head->r;
          delete _head;
          _head = _it_ptr;
        } else {
          delete _head;
          _head = nullptr;
        }

        rm_count ++;
        break;
      } else _head = _head->r;
    }

    _count_nodes -= rm_count;
  }

  _m_lock.unlock();
}

void RRList::rm_all(int fd) {
  _m_lock.lock();
  int rm_count = 0;
  
  if(_head != nullptr) {
    for(int i = 0; i < _count_nodes && _count_nodes - rm_count > 0; i ++) {
      if(_head->fd == fd) {
        if(_count_nodes - rm_count > 1) {
          _head->r->l = _head->l;
          _head->l->r = _head->r;
          _it_ptr = _head->r;

          delete _head;
          _head = _it_ptr;
        } else {
          delete _head;
          _head = nullptr;
        }

        rm_count ++;
      } else _head = _head->r;
    }

    _count_nodes -= rm_count;
  }

  _m_lock.unlock();
}

void RRList::print() {
  _m_lock.lock();

  _it_ptr = _head;
  for(int i = 0; i < _count_nodes; i ++) {
    _it_ptr = _it_ptr->r;
  }

  _m_lock.unlock();
}

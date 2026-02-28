#pragma once

#ifdef DEBUG
#include "mutex_debug.hpp"
#else
#include "mutex_ndebug.hpp"
#endif

typedef pthread_mutex_t mutex_t;

// Simple mutex_t wrapper to provide RAII
// Because macros are needed to get location in the code a lock is
// called not making it std::mutex compatible.
class Mutex
{
private:
  mutex_t _mutex;

public:
  Mutex()
  {
    mutex_init(_mutex);
  }

  ~Mutex()
  {
    mutex_destroy(_mutex);
  }

  operator mutex_t&()
  {
    return _mutex;
  }
};

class LockGuard
{
private:
  mutex_t &_mutex;

public:
  LockGuard(mutex_t &mutex_)
    : _mutex(mutex_)
  {
    mutex_lock(_mutex);
  }

  ~LockGuard()
  {
    mutex_unlock(_mutex);
  }
};

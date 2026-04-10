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

// RAII mutex guard macro.  Expands to a single variable declaration,
// so it is safe in unbraced if/else/for/while.
// In debug builds the call-site __FILE__/__func__/__LINE__ are
// forwarded to _mutex_lock/_mutex_unlock for diagnostics.
#define MUTEX_LOCKGUARD_CONCAT_(a,b) a##b
#define MUTEX_LOCKGUARD_CONCAT(a,b)  MUTEX_LOCKGUARD_CONCAT_(a,b)

#ifdef DEBUG
#define mutex_lockguard(m) \
  ScopedMutexLock MUTEX_LOCKGUARD_CONCAT(_mtx_guard_,__COUNTER__) \
    ((m),__FILE__,__func__,__LINE__)
#else
#define mutex_lockguard(m) \
  ScopedMutexLock MUTEX_LOCKGUARD_CONCAT(_mtx_guard_,__COUNTER__)(m)
#endif

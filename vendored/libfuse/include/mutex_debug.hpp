#pragma once

#include "fmt/core.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if defined(__has_include)
# if __has_include(<features.h>)
#  include <features.h>
# endif
#endif

#include <pthread.h>

#ifndef PTHREAD_MUTEX_ADAPTIVE_NP
# define PTHREAD_MUTEX_ADAPTIVE_NP PTHREAD_MUTEX_NORMAL
#endif

#if defined(_GNU_SOURCE) && defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2,30)
#  define MUTEX_HAS_PTHREAD_MUTEX_CLOCKLOCK 1
# endif
#endif

#define mutex_init(M)    _mutex_init((M),__FILE__,__func__,__LINE__)
#define mutex_destroy(M) _mutex_destroy((M),__FILE__,__func__,__LINE__)
#define mutex_lock(M)    _mutex_lock((M),__FILE__,__func__,__LINE__)
#define mutex_unlock(M)  _mutex_unlock((M),__FILE__,__func__,__LINE__)


static
inline
void
_print_error_and_abort(const char *func1_,
                       const int   errno_,
                       const char *file_,
                       const char *func2_,
                       const int   linenum_)
{
  fmt::println(stderr,
               "ERROR: {} returned {} ({}) - ({}:{}:{})",
               func1_,
               errno_,
               strerror(errno_),
               file_,
               func2_,
               linenum_);
  std::abort();
}

static
inline
void
_mutex_init(pthread_mutex_t &mutex_,
            const char      *file_,
            const char      *func_,
            const int        linenum_)
{
  int rv;
  pthread_mutexattr_t attr;

  rv = pthread_mutexattr_init(&attr);
  if(rv != 0)
    _print_error_and_abort("pthread_mutexattr_init",rv,file_,func_,linenum_);

  rv = pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ADAPTIVE_NP);
  if(rv != 0)
    {
      pthread_mutexattr_destroy(&attr);
      _print_error_and_abort("pthread_mutexattr_settype",rv,file_,func_,linenum_);
    }

  rv = pthread_mutex_init(&mutex_,&attr);
  if(rv != 0)
    {
      pthread_mutexattr_destroy(&attr);
      _print_error_and_abort("pthread_mutex_init",rv,file_,func_,linenum_);
    }

  rv = pthread_mutexattr_destroy(&attr);
  if(rv != 0)
    _print_error_and_abort("pthread_mutexattr_destroy",rv,file_,func_,linenum_);
}

static
inline
void
_mutex_destroy(pthread_mutex_t &mutex_,
               const char      *file_,
               const char      *func_,
               const int        linenum_)
{
  int rv;

  rv = pthread_mutex_destroy(&mutex_);
  if(rv != 0)
    _print_error_and_abort(__func__,rv,file_,func_,linenum_);
}

static
inline
void
_mutex_lock(pthread_mutex_t &mutex_,
            const char      *file_,
            const char      *func_,
            const int        linenum_)
{
  int rv;
  int cnt = 0;
  timespec timeout;

  while(true)
    {
#ifdef MUTEX_HAS_PTHREAD_MUTEX_CLOCKLOCK
      rv = clock_gettime(CLOCK_MONOTONIC,&timeout);
#else
      rv = clock_gettime(CLOCK_REALTIME,&timeout);
#endif
      if(rv != 0)
        _print_error_and_abort("clock_gettime",errno,file_,func_,linenum_);

      timeout.tv_nsec += 1000000 * 1; // 1ms
      if(timeout.tv_nsec >= 1000000000)
        {
          timeout.tv_sec += 1;
          timeout.tv_nsec -= 1000000000;
        }

#ifdef MUTEX_HAS_PTHREAD_MUTEX_CLOCKLOCK
      rv = pthread_mutex_clocklock(&mutex_,CLOCK_MONOTONIC,&timeout);
#else
      rv = pthread_mutex_timedlock(&mutex_,&timeout);
#endif
      switch(rv)
        {
        case 0:
          return;
        case ETIMEDOUT:
          cnt++;
          fmt::println(stderr,
                       "NOTICE: mutex timed lock expired - count={}; ({}:{}:{})",
                       cnt,
                       file_,
                       func_,
                       linenum_);
          continue;
        default:
          _print_error_and_abort(__func__,rv,file_,func_,linenum_);
          return;
        }
    }
}

static
inline
void
_mutex_unlock(pthread_mutex_t &mutex_,
              const char      *file_,
              const char      *func_,
              const int        linenum_)
{
  int rv;

  rv = pthread_mutex_unlock(&mutex_);
  if(rv != 0)
    _print_error_and_abort(__func__,rv,file_,func_,linenum_);
}

class ScopedMutexLock
{
private:
  pthread_mutex_t &_mutex;
  const char      *_file;
  const char      *_func;
  int              _line;

public:
  explicit
  ScopedMutexLock(pthread_mutex_t &mutex_,
                  const char      *file_,
                  const char      *func_,
                  int              line_)
    : _mutex(mutex_), _file(file_), _func(func_), _line(line_)
  {
    _mutex_lock(_mutex,_file,_func,_line);
  }

  ~ScopedMutexLock() noexcept
  {
    _mutex_unlock(_mutex,_file,_func,_line);
  }

  ScopedMutexLock(const ScopedMutexLock&) = delete;
  ScopedMutexLock& operator=(const ScopedMutexLock&) = delete;
};

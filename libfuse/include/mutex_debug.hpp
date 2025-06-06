#pragma once

#include "fmt/core.h"

#include <cstdlib>

#include <pthread.h>
#include <time.h>

#ifndef PTHREAD_MUTEX_ADAPTIVE_NP
# define PTHREAD_MUTEX_ADAPTIVE_NP PTHREAD_MUTEX_NORMAL
#endif

#define mutex_init(M)    _mutex_init(M,__FILE__,__func__,__LINE__)
#define mutex_destroy(M) _mutex_destroy(M,__FILE__,__func__,__LINE__)
#define mutex_lock(M)    _mutex_lock(M,__FILE__,__func__,__LINE__)
#define mutex_unlock(M)  _mutex_unlock(M,__FILE__,__func__,__LINE__)


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
  abort();
}

static
inline
void
_mutex_init(pthread_mutex_t *mutex_,
            const char      *file_,
            const char      *func_,
            const int        linenum_)
{
  int rv;
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ADAPTIVE_NP);

  rv = pthread_mutex_init(mutex_,&attr);
  if(rv != 0)
    _print_error_and_abort(__func__,rv,file_,func_,linenum_);

  pthread_mutexattr_destroy(&attr);
}

static
inline
void
_mutex_destroy(pthread_mutex_t *mutex_,
               const char      *file_,
               const char      *func_,
               const int        linenum_)
{
  int rv;

  rv = pthread_mutex_destroy(mutex_);
  if(rv != 0)
    _print_error_and_abort(__func__,rv,file_,func_,linenum_);
}

static
inline
void
_mutex_lock(pthread_mutex_t *mutex_,
            const char      *file_,
            const char      *func_,
            const int        linenum_)
{
  int rv;
  int cnt = 0;
  timespec timeout;

  while(true)
    {
      clock_gettime(CLOCK_REALTIME,&timeout);
      timeout.tv_nsec += 1000000 * 1; // 1ms
      if(timeout.tv_nsec >= 1000000000)
        {
          timeout.tv_sec += 1;
          timeout.tv_nsec -= 1000000000;
        }

      rv = pthread_mutex_timedlock(mutex_,&timeout);
      switch(rv)
        {
        case 0:
          return;
        case ETIMEDOUT:
          cnt++;
          fmt::println(stderr,
                       "NOTICE: pthread_mutex_timedlock expired - count={}; ({}:{}:{})",
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
_mutex_unlock(pthread_mutex_t *mutex_,
              const char      *file_,
              const char      *func_,
              const int        linenum_)
{
  int rv;

  rv = pthread_mutex_unlock(mutex_);
  if(rv != 0)
    _print_error_and_abort(__func__,rv,file_,func_,linenum_);
}

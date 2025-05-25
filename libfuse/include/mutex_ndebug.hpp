#pragma once

#include <cstdlib>

#include <pthread.h>


static
inline
void
mutex_init(pthread_mutex_t *mutex_)
{
  int rv;
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ADAPTIVE_NP);

  rv = pthread_mutex_init(mutex_,&attr);
  if(rv != 0)
    abort();

  pthread_mutexattr_destroy(&attr);
}

static
inline
void
mutex_lock(pthread_mutex_t *mutex_)
{
  int rv;

  rv = pthread_mutex_lock(mutex_);
  if(rv != 0)
    abort();
}

static
inline
void
mutex_unlock(pthread_mutex_t *mutex_)
{
  int rv;

  rv = pthread_mutex_unlock(mutex_);
  if(rv != 0)
    abort();
}

static
inline
void
mutex_destroy(pthread_mutex_t *mutex_)
{
  int rv;

  rv = pthread_mutex_destroy(mutex_);
  if(rv != 0)
    abort();
}

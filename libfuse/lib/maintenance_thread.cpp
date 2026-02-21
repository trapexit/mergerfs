#include "maintenance_thread.hpp"

#include "mutex.hpp"

#include "fmt/core.h"

#include <cassert>
#include <vector>

#include <pthread.h>
#include <unistd.h>


pthread_t g_thread;
std::vector<std::function<void(int)>> g_funcs;
mutex_t g_mutex;

static
void*
_thread_loop(void *)
{
  int count;

  pthread_setname_np(pthread_self(),"fuse.maint");

  count = 0;
  while(true)
    {
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
      {
        LockGuard lg(g_mutex);
        for(auto &func : g_funcs)
          func(count);
      }
      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);

      count++;
      ::sleep(60);
    }

  return NULL;
}

void
MaintenanceThread::setup()
{
  int rv;

  rv = pthread_create(&g_thread,NULL,_thread_loop,NULL);
  assert((rv == 0) && "pthread_create failed");
}

void
MaintenanceThread::push_job(const std::function<void(int)> &func_)
{
  LockGuard lg(g_mutex);

  g_funcs.emplace_back(func_);
}

void
MaintenanceThread::stop()
{
  pthread_cancel(g_thread);
  pthread_join(g_thread,NULL);
}

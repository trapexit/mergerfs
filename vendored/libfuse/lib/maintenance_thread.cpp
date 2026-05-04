#include "maintenance_thread.hpp"

#include "fatal.hpp"
#include "mutex.hpp"

#include <vector>

#include <pthread.h>
#include <unistd.h>

static pthread_t g_thread;
static std::vector<std::function<void(u64)>> g_funcs;
static Mutex g_mutex;

static
void*
_thread_loop(void *)
{
  u64 count;

  pthread_setname_np(pthread_self(),"fuse.maint");

  count = 0;
  while(true)
    {
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
      {
        std::vector<std::function<void(u64)>> funcs;

        {
          mutex_lockguard(g_mutex);
          funcs = g_funcs;
        }

        for(auto &func : funcs)
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
  if(rv != 0)
    fatal::abort("pthread_create failed: rv={}",rv);
}

void
MaintenanceThread::push_job(const std::function<void(u64)> &func_)
{
  mutex_lockguard(g_mutex);

  g_funcs.emplace_back(func_);
}

void
MaintenanceThread::stop()
{
  pthread_cancel(g_thread);
  pthread_join(g_thread,NULL);

  {
    mutex_lockguard(g_mutex);
    g_funcs.clear();
  }
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpu.hpp"
#include "pin_threads.hpp"
#include "fmt/core.h"
#include "scope_guard.hpp"
#include "thread_pool.hpp"
#include "syslog.hpp"
#include "maintenance_thread.hpp"

#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"

#include "fuse_config.hpp"
#include "fuse_msgbuf.hpp"
#include "fuse_ll.hpp"

#include <cassert>
#include <memory>
#include <vector>

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>


static
bool
_retriable_receive_error(const int err_)
{
  switch(err_)
    {
    case -EINTR:
    case -EAGAIN:
    case -ENOENT:
      return true;
    default:
      return false;
    }
}

static
void
_print_error(int rv_)
{
  fmt::print(stderr,
             "mergerfs: error reading from /dev/fuse - {} ({})\n",
             strerror(-rv_),
             -rv_);
}

struct AsyncWorker
{
  fuse_session *_se;
  sem_t *_finished;
  std::shared_ptr<ThreadPool> _process_tp;

  AsyncWorker(fuse_session                *se_,
              sem_t                       *finished_,
              std::shared_ptr<ThreadPool>  process_tp_)
    : _se(se_),
      _finished(finished_),
      _process_tp(process_tp_)
  {
  }

  inline
  void
  operator()() const
  {
    DEFER{ fuse_session_exit(_se); };
    DEFER{ sem_post(_finished); };

    ThreadPool::PToken ptok(_process_tp->ptoken());
    while(!fuse_session_exited(_se))
      {
        int rv;
        fuse_msgbuf_t *msgbuf;

        msgbuf = msgbuf_alloc();

        while(true)
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,msgbuf);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);

            if(rv > 0)
              break;
            if(::_retriable_receive_error(rv))
              continue;

            msgbuf_free(msgbuf);
            if((rv == 0) || (rv == -ENODEV))
              return;

            return ::_print_error(rv);
          }

        _process_tp->enqueue_work(ptok,
                                  [=]()
                                  {
                                    _se->process_buf(_se,msgbuf);
                                    msgbuf_free(msgbuf);
                                  });
      }
  }
};

struct SyncWorker
{
  fuse_session *_se;
  sem_t *_finished;

  SyncWorker(fuse_session *se_,
             sem_t        *finished_)

    : _se(se_),
      _finished(finished_)
  {
  }

  inline
  void
  operator()() const
  {
    DEFER{ fuse_session_exit(_se); };
    DEFER{ sem_post(_finished); };

    while(!fuse_session_exited(_se))
      {
        int rv;
        fuse_msgbuf_t *msgbuf;

        msgbuf = msgbuf_alloc();

        while(true)
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,msgbuf);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
            if(rv > 0)
              break;
            if(::_retriable_receive_error(rv))
              continue;

            msgbuf_free(msgbuf);
            if((rv == 0) || (rv == -ENODEV))
              return;

            return ::_print_error(rv);
          }

        _se->process_buf(_se,msgbuf);

        msgbuf_free(msgbuf);
      }
  }
};

int
fuse_start_thread(pthread_t *thread_id,
                  void      *(*func)(void *),
                  void      *arg)
{
  int res;
  sigset_t oldset;
  sigset_t newset;

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);
  res = pthread_create(thread_id,NULL,func,arg);
  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(res != 0)
    {
      fprintf(stderr,
              "fuse: error creating thread: %s\n",
              strerror(res));
      return -1;
    }

  return 0;
}

static
int
_calculate_thread_count(const int raw_thread_count_)
{
  int thread_count = 1;

  if(raw_thread_count_ == 0)
    {
      thread_count = std::thread::hardware_concurrency();
      thread_count = std::min(8,thread_count);
    }
  else if(raw_thread_count_ < 0)
    {
      thread_count = (std::thread::hardware_concurrency() / -raw_thread_count_);
      thread_count = std::min(1,thread_count);
    }
  else if(raw_thread_count_ > 0)
    {
      thread_count = raw_thread_count_;
    }

  return thread_count;
}

static
void
_calculate_thread_counts(int *read_thread_count_,
                         int *process_thread_count_,
                         int *process_thread_queue_depth_)
{
  if((*read_thread_count_ == 0) && (*process_thread_count_ == -1))
    {
      int nproc;

      nproc = std::thread::hardware_concurrency();
      *read_thread_count_ = std::min(8,nproc);
    }
  else if((*read_thread_count_ == 0) && (*process_thread_count_ == 0))
    {
      int nproc;

      nproc = std::thread::hardware_concurrency();
      *read_thread_count_ = 2;
      *process_thread_count_ = std::max(2,(nproc - 2));
      *process_thread_count_ = std::min(8,*process_thread_count_);
    }
  else if((*read_thread_count_ == 0) && (*process_thread_count_ != -1))
    {
      *read_thread_count_    = 2;
      *process_thread_count_ = ::_calculate_thread_count(*process_thread_count_);
    }
  else
    {
      *read_thread_count_    = ::_calculate_thread_count(*read_thread_count_);
      if(*process_thread_count_ != -1)
        *process_thread_count_ = ::_calculate_thread_count(*process_thread_count_);
    }

  if(*process_thread_queue_depth_ <= 0)
    *process_thread_queue_depth_ = 2;

  *process_thread_queue_depth_ *= std::abs(*process_thread_count_);
}

int
fuse_session_loop_mt(struct fuse_session *se_,
                     const int            raw_read_thread_count_,
                     const int            raw_process_thread_count_,
                     const int            raw_process_thread_queue_depth_,
                     const std::string    pin_threads_type_)
{
  sem_t finished;
  int read_thread_count;
  int process_thread_count;
  int process_thread_queue_depth;
  std::vector<pthread_t> read_threads;
  std::vector<pthread_t> process_threads;
  std::unique_ptr<ThreadPool> read_tp;
  std::shared_ptr<ThreadPool> process_tp;

  sem_init(&finished,0,0);

  read_thread_count          = raw_read_thread_count_;
  process_thread_count       = raw_process_thread_count_;
  process_thread_queue_depth = raw_process_thread_queue_depth_;
  ::_calculate_thread_counts(&read_thread_count,
                             &process_thread_count,
                             &process_thread_queue_depth);

  if(process_thread_count > 0)
    process_tp = std::make_shared<ThreadPool>(process_thread_count,
                                              process_thread_queue_depth,
                                              "fuse.process");

  read_tp = std::make_unique<ThreadPool>(read_thread_count,
                                         read_thread_count,
                                         "fuse.read");
  if(process_tp)
    {
      for(auto i = 0; i < read_thread_count; i++)
        read_tp->enqueue_work(AsyncWorker(se_,&finished,process_tp));
    }
  else
    {
      for(auto i = 0; i < read_thread_count; i++)
        read_tp->enqueue_work(SyncWorker(se_,&finished));
    }

  if(read_tp)
    read_threads = read_tp->threads();
  if(process_tp)
    process_threads = process_tp->threads();

  PinThreads::pin(read_threads,process_threads,pin_threads_type_);

  SysLog::info("read-thread-count={}; "
               "process-thread-count={}; "
               "process-thread-queue-depth={}; "
               "pin-threads={};",
               read_thread_count,
               process_thread_count,
               process_thread_queue_depth,
               pin_threads_type_);

  while(!fuse_session_exited(se_))
    sem_wait(&finished);

  sem_destroy(&finished);

  return 0;
}

int
fuse_loop_mt(struct fuse *f_)
{
  int res;

  if(f_ == NULL)
    return -1;

  MaintenanceThread::setup();
  fuse_populate_maintenance_thread(f_);

  res = fuse_session_loop_mt(fuse_get_session(f_),
                             fuse_config_get_read_thread_count(),
                             fuse_config_get_process_thread_count(),
                             fuse_config_get_process_thread_queue_depth(),
                             fuse_config_get_pin_threads());

  MaintenanceThread::stop();

  return res;
}

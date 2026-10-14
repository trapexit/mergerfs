#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpu.hpp"
#include "pin_threads.hpp"
#include "fmt/core.h"
#include "scope_guard/scope_guard.hpp"
#include "thread_pool.hpp"
#include "syslog.hpp"
#include "maintenance_thread.hpp"

#include "fuse_i.hpp"
#include "fuse_lowlevel.h"

#include "fuse_cfg.hpp"
#include "fuse_msgbuf.hpp"

#include <cassert>
#include <memory>
#include <vector>

#include <errno.h>
#include <poll.h>
#include <time.h>
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

static
int
_nproc()
{
  return std::max(1,(int)std::thread::hardware_concurrency());
}

struct AsyncWorker
{
  fuse_session *_se;
  int _fd;
  sem_t *_finished;
  std::shared_ptr<ThreadPool> _process_tp;

  AsyncWorker(fuse_session                *se_,
              int                          fd_,
              sem_t                       *finished_,
              std::shared_ptr<ThreadPool>  process_tp_)
    : _se(se_),
      _fd(fd_),
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
        if(not msgbuf)
          {
            fmt::println(stderr,"mergerfs: failure to allocate msgbuf, exiting");
            return;
          }

        while(true)
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,_fd,msgbuf);
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
                                  [se = _se,fd = _fd,msgbuf]()
                                  {
                                    se->process_buf(se,fd,msgbuf);
                                    msgbuf_free(msgbuf);
                                  });
      }
  }
};

struct SyncWorker
{
  fuse_session *_se;
  int _fd;
  sem_t *_finished;
  u64 _spin_us;

  SyncWorker(fuse_session *se_,
             int           fd_,
             sem_t        *finished_,
             u64           spin_us_)

    : _se(se_),
      _fd(fd_),
      _finished(finished_),
      _spin_us(spin_us_)
  {
  }

  inline
  void
  operator()() const
  {
    DEFER{ fuse_session_exit(_se); };
    DEFER{ sem_post(_finished); };

    bool processed_one = false;
    while(!fuse_session_exited(_se))
      {
        int rv;
        fuse_msgbuf_t *msgbuf;

        msgbuf = msgbuf_alloc();
        if(not msgbuf)
          {
            fmt::println(stderr,"mergerfs: failure to allocate msgbuf, exiting");
            return;
          }

        /* After processing a request, poll /dev/fuse for up to _spin_us
           before blocking in receive. On synchronous request streams (one
           dd, one metadata walk) this measurably lowers per-request latency
           versus blocking immediately; the exact scheduler mechanism is not
           established. Cost: up to _spin_us of CPU per worker after each
           request, so a busy-but-quiet mount pays roughly a core; nothing on
           an idle mount. 0 disables. Cancellation stays disabled during the
           spin (it is only enabled around the blocking receive) and the spin
           is bounded, so shutdown is unaffected. */
        if(processed_one && (_spin_us > 0))
          {
            struct pollfd pfd = {_fd,POLLIN,0};
            struct timespec ts;
            uint64_t end;

            ::clock_gettime(CLOCK_MONOTONIC,&ts);
            end = ((uint64_t)ts.tv_sec * 1000000000ULL +
                   (uint64_t)ts.tv_nsec +
                   (_spin_us * 1000ULL));
            while(::poll(&pfd,1,0) == 0)
              {
                ::clock_gettime(CLOCK_MONOTONIC,&ts);
                if(((uint64_t)ts.tv_sec * 1000000000ULL +
                    (uint64_t)ts.tv_nsec) >= end)
                  break;
              }
          }
        processed_one = true;

        while(true)
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,_fd,msgbuf);
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

        _se->process_buf(_se,_fd,msgbuf);

        msgbuf_free(msgbuf);
      }
  }
};

static
int
_calculate_thread_count(const int raw_thread_count_)
{
  int thread_count = 1;

  if(raw_thread_count_ == 0)
    {
      thread_count = ::_nproc();
      thread_count = std::min(8,thread_count);
    }
  else if(raw_thread_count_ < 0)
    {
      thread_count = (::_nproc() / -raw_thread_count_);
      thread_count = std::max(1,thread_count);
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

      nproc = ::_nproc();
      *read_thread_count_ = std::min(8,nproc);
    }
  else if((*read_thread_count_ == 0) && (*process_thread_count_ == 0))
    {
      int nproc;

      nproc = ::_nproc();
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
                     const std::string    pin_threads_type_,
                     const u64            read_spin_us_)
{
  sem_t finished;
  bool cloned_read_fds;
  int read_thread_count;
  int process_thread_count;
  int process_thread_queue_depth;
  std::vector<pthread_t> read_threads;
  std::vector<pthread_t> process_threads;

  sem_init(&finished,0,0);
  DEFER{ sem_destroy(&finished); };

  std::unique_ptr<ThreadPool> read_tp;
  std::shared_ptr<ThreadPool> process_tp;

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

  cloned_read_fds = fuse_session_setup_read_fds(se_,read_thread_count);

  read_tp = std::make_unique<ThreadPool>(read_thread_count,
                                          read_thread_count,
                                          "fuse.read");
  if(process_tp)
    {
      for(auto i = 0; i < read_thread_count; i++)
        read_tp->enqueue_work(AsyncWorker(se_,
                                          fuse_session_read_fd(se_,i),
                                          &finished,
                                          process_tp));
    }
  else
    {
      for(auto i = 0; i < read_thread_count; i++)
        read_tp->enqueue_work(SyncWorker(se_,
                                         fuse_session_read_fd(se_,i),
                                         &finished,
                                         read_spin_us_));
    }

  if(read_tp)
    read_threads = read_tp->threads();
  if(process_tp)
    process_threads = process_tp->threads();

  PinThreads::pin(read_threads,process_threads,pin_threads_type_);

  SysLog::info("read-thread-count={}; "
               "process-thread-count={}; "
               "process-thread-queue-depth={}; "
               "fuse-dev-ioc-clone={}; "
               "pin-threads={}; "
               "read-spin-us={};",
               read_thread_count,
               process_thread_count,
               process_thread_queue_depth,
               cloned_read_fds,
               pin_threads_type_,
               read_spin_us_);

  while(!fuse_session_exited(se_))
    sem_wait(&finished);

  for(auto t : read_threads)
    pthread_cancel(t);

  read_tp.reset();
  process_tp.reset();

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

  res = fuse_session_loop_mt(fuse_get_session(),
                             fuse_cfg.read_thread_count,
                             fuse_cfg.process_thread_count,
                             fuse_cfg.process_thread_queue_depth,
                             fuse_cfg.pin_threads,
                             fuse_cfg.read_spin_us);

  MaintenanceThread::stop();

  return res;
}

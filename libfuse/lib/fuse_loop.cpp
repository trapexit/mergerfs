#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpu.hpp"
#include "fmt/core.h"
#include "make_unique.hpp"
#include "scope_guard.hpp"
#include "thread_pool.hpp"

#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"

#include "fuse_config.hpp"
#include "fuse_msgbuf.hpp"
#include "fuse_ll.hpp"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include <cassert>
#include <vector>

static
bool
retriable_receive_error(const int err_)
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
bool
fatal_receive_error(const int err_)
{
  return (err_ < 0);
}

static
void
handle_receive_error(const int      rv_,
                     fuse_msgbuf_t *msgbuf_)
{
  msgbuf_free(msgbuf_);

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

  AsyncWorker(fuse_session                       *se_,
              sem_t                              *finished_,
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

    moodycamel::ProducerToken ptok(_process_tp->ptoken());
    while(!fuse_session_exited(_se))
      {
        int rv;
        fuse_msgbuf_t *msgbuf;

        msgbuf = msgbuf_alloc();

        do
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,msgbuf);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
            if(rv == 0)
              return;
            if(retriable_receive_error(rv))
              continue;
            if(fatal_receive_error(rv))
              return handle_receive_error(rv,msgbuf);
          } while(false);

        auto const func = [=]
        {
          _se->process_buf(_se,msgbuf);
          msgbuf_free(msgbuf);
        };

        _process_tp->enqueue_work(ptok,func);
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

        do
          {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            rv = _se->receive_buf(_se,msgbuf);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
            if(rv == 0)
              return;
            if(retriable_receive_error(rv))
              continue;
            if(fatal_receive_error(rv))
              return handle_receive_error(rv,msgbuf);
          } while(false);

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
calculate_thread_count(const int raw_thread_count_)
{
  int thread_count;

  thread_count = 4;
  if(raw_thread_count_ == 0)
    thread_count = std::thread::hardware_concurrency();
  else if(raw_thread_count_ < 0)
    thread_count = (std::thread::hardware_concurrency() / -raw_thread_count_);
  else if(raw_thread_count_ > 0)
    thread_count = raw_thread_count_;

  if(thread_count <= 0)
    thread_count = 1;

  return thread_count;
}

static
void
calculate_thread_counts(int *read_thread_count_,
                        int *process_thread_count_,
                        int *process_thread_queue_depth_)
{
  if((*read_thread_count_ == -1) && (*process_thread_count_ == -1))
    {
      int nproc;

      nproc = std::thread::hardware_concurrency();
      *read_thread_count_ = 2;
      *process_thread_count_ = std::max(2,(nproc - 2));
    }
  else
    {
      *read_thread_count_ = ::calculate_thread_count(*read_thread_count_);
      if(*process_thread_count_ != -1)
        *process_thread_count_ = ::calculate_thread_count(*process_thread_count_);
    }

  if(*process_thread_queue_depth_ <= 0)
    *process_thread_queue_depth_ = *process_thread_count_;
}

static
void
pin_threads_R1L(const CPU::ThreadIdVec read_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,cpus.front());
}

static
void
pin_threads_R1P(const CPU::ThreadIdVec read_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}

static
void
pin_threads_RP1L(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,cpus.front());
  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,cpus.front());
}

static
void
pin_threads_RP1P(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}

static
void
pin_threads_R1LP1L(const std::vector<pthread_t> read_threads_,
                   const std::vector<pthread_t> process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,cpus.front());

  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,cpus.back());
}

static
void
pin_threads_R1PP1P(const std::vector<pthread_t> read_threads_,
                   const std::vector<pthread_t> process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);

  if(core2cpus.size() > 1)
    core2cpus.erase(core2cpus.begin());

  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}

static
void
pin_threads_RPSL(const std::vector<pthread_t> read_threads_,
                 const std::vector<pthread_t> process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    {
      if(cpus.empty())
        cpus = CPU::cpus();
      CPU::setaffinity(thread_id,cpus.back());
      cpus.pop_back();
    }

  for(auto const thread_id : process_threads_)
    {
      if(cpus.empty())
        cpus = CPU::cpus();
      CPU::setaffinity(thread_id,cpus.back());
      cpus.pop_back();
    }
}

static
void
pin_threads_RPSP(const std::vector<pthread_t> read_threads_,
                 const std::vector<pthread_t> process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    {
      if(core2cpus.empty())
        core2cpus = CPU::core2cpus();
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }

  for(auto const thread_id : process_threads_)
    {
      if(core2cpus.empty())
        core2cpus = CPU::core2cpus();
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }
}

static
void
pin_threads_R1PPSP(const std::vector<pthread_t> read_threads_,
                   const std::vector<pthread_t> process_threads_)
{
  CPU::Core2CPUsMap core2cpus;
  CPU::Core2CPUsMap leftover;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);

  core2cpus.erase(core2cpus.begin());
  if(core2cpus.empty())
    core2cpus = CPU::core2cpus();
  leftover = core2cpus;

  for(auto const thread_id : process_threads_)
    {
      if(core2cpus.empty())
        core2cpus = leftover;
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }
}

static
void
pin_threads(const std::vector<pthread_t> read_threads_,
            const std::vector<pthread_t> process_threads_,
            const std::string            type_)
{
  if(type_.empty() || (type_ == "false"))
    return;
  if(type_ == "R1L")
    return ::pin_threads_R1L(read_threads_);
  if(type_ == "R1P")
    return ::pin_threads_R1P(read_threads_);
  if(type_ == "RP1L")
    return ::pin_threads_RP1L(read_threads_,process_threads_);
  if(type_ == "RP1P")
    return ::pin_threads_RP1P(read_threads_,process_threads_);
  if(type_ == "R1LP1L")
    return ::pin_threads_R1LP1L(read_threads_,process_threads_);
  if(type_ == "R1PP1P")
    return ::pin_threads_R1PP1P(read_threads_,process_threads_);
  if(type_ == "RPSL")
    return ::pin_threads_RPSL(read_threads_,process_threads_);
  if(type_ == "RPSP")
    return ::pin_threads_RPSP(read_threads_,process_threads_);
  if(type_ == "R1PPSP")
    return ::pin_threads_R1PPSP(read_threads_,process_threads_);

  syslog(LOG_WARNING,
         "Invalid pin-threads value, ignoring: %s",
         type_.c_str());
}

static
void
wait(fuse_session *se_,
     sem_t        *finished_sem_)
{
  while(!fuse_session_exited(se_))
    sem_wait(finished_sem_);
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
  ::calculate_thread_counts(&read_thread_count,
                            &process_thread_count,
                            &process_thread_queue_depth);

  if(process_thread_count > 0)
    process_tp = std::make_shared<ThreadPool>(process_thread_count,
                                              (process_thread_count *
                                               process_thread_queue_depth),
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

  ::pin_threads(read_threads,process_threads,pin_threads_type_);

  syslog(LOG_INFO,
         "read-thread-count=%d; "
         "process-thread-count=%d; "
         "process-thread-queue-depth=%d; "
         "pin-threads=%s;"
         ,
         read_thread_count,
         process_thread_count,
         process_thread_queue_depth,
         pin_threads_type_.c_str());

  ::wait(se_,&finished);

  sem_destroy(&finished);

  return 0;
}

int
fuse_loop_mt(struct fuse *f)
{
  if(f == NULL)
    return -1;

  int res = fuse_start_maintenance_thread(f);
  if(res)
    return -1;

  res = fuse_session_loop_mt(fuse_get_session(f),
                             fuse_config_get_read_thread_count(),
                             fuse_config_get_process_thread_count(),
                             fuse_config_get_process_thread_queue_depth(),
                             fuse_config_get_pin_threads());

  fuse_stop_maintenance_thread(f);

  return res;
}

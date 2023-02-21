#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "thread_pool.hpp"
#include "cpu.hpp"
#include "fmt/core.h"

#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"

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
#include <unistd.h>

#include <cassert>
#include <vector>


struct fuse_worker_data_t
{
  struct fuse_session *se;
  sem_t finished;
  std::function<void(fuse_worker_data_t*,fuse_msgbuf_t*)> msgbuf_processor;
  std::function<fuse_msgbuf_t*(void)> msgbuf_allocator;
  std::shared_ptr<ThreadPool> tp;
};

class WorkerCleanup
{
public:
  WorkerCleanup(fuse_worker_data_t *wd_)
    : _wd(wd_)
  {
  }

  ~WorkerCleanup()
  {
    fuse_session_exit(_wd->se);
    sem_post(&_wd->finished);
  }

private:
  fuse_worker_data_t *_wd;
};

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
void*
handle_receive_error(const int      rv_,
                     fuse_msgbuf_t *msgbuf_)
{
  msgbuf_free(msgbuf_);

  fprintf(stderr,
          "mergerfs: error reading from /dev/fuse - %s (%d)\n",
          strerror(-rv_),
          -rv_);

  return NULL;
}

static
void*
fuse_do_work(void *data)
{
  fuse_worker_data_t *wd = (fuse_worker_data_t*)data;
  fuse_session       *se = wd->se;
  auto               &process_msgbuf = wd->msgbuf_processor;
  auto               &msgbuf_allocator = wd->msgbuf_allocator;
  WorkerCleanup       workercleanup(wd);

  while(!fuse_session_exited(se))
    {
      int rv;
      fuse_msgbuf_t *msgbuf;

      msgbuf = msgbuf_allocator();

      do
        {
          pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
          rv = se->receive_buf(se,msgbuf);
          pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
          if(rv == 0)
            return NULL;
          if(retriable_receive_error(rv))
            continue;
          if(fatal_receive_error(rv))
            return handle_receive_error(rv,msgbuf);
        } while(false);

      process_msgbuf(wd,msgbuf);
    }

  return NULL;
}

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
                        int *process_thread_count_)
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
}

static
void
process_msgbuf_sync(fuse_worker_data_t *wd_,
                    fuse_msgbuf_t      *msgbuf_)
{
  wd_->se->process_buf(wd_->se,msgbuf_);
  msgbuf_free(msgbuf_);
}

static
void
process_msgbuf_async(fuse_worker_data_t *wd_,
                     fuse_msgbuf_t      *msgbuf_)
{
  const auto func = [=] {
    process_msgbuf_sync(wd_,msgbuf_);
  };

  wd_->tp->enqueue_work(func);
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
pin_threads(const std::vector<pthread_t>  read_threads_,
            const std::vector<pthread_t>  process_threads_,
            const std::string             type_)
{
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
}

int
fuse_session_loop_mt(struct fuse_session *se_,
                     const int            raw_read_thread_count_,
                     const int            raw_process_thread_count_,
                     const char          *pin_threads_type_)
{
  int err;
  int read_thread_count;
  int process_thread_count;
  fuse_worker_data_t wd = {0};
  std::vector<pthread_t> read_threads;
  std::vector<pthread_t> process_threads;

  read_thread_count    = raw_read_thread_count_;
  process_thread_count = raw_process_thread_count_;
  ::calculate_thread_counts(&read_thread_count,&process_thread_count);

  if(process_thread_count > 0)
    {
      wd.tp = std::make_shared<ThreadPool>(process_thread_count);
      wd.msgbuf_processor = process_msgbuf_async;
      process_threads = wd.tp->threads();
    }
  else
    {
      wd.msgbuf_processor = process_msgbuf_sync;
    }

  wd.msgbuf_allocator = ((se_->f->splice_read) ? msgbuf_alloc : msgbuf_alloc_memonly);

  wd.se = se_;
  sem_init(&wd.finished,0,0);

  err = 0;
  for(int i = 0; i < read_thread_count; i++)
    {
      pthread_t thread_id;
      err = fuse_start_thread(&thread_id,fuse_do_work,&wd);
      assert(err == 0);
      read_threads.push_back(thread_id);
    }

  if(pin_threads_type_ != NULL)
    ::pin_threads(read_threads,process_threads,pin_threads_type_);

  if(!err)
    {
      /* sem_wait() is interruptible */
      while(!fuse_session_exited(se_))
        sem_wait(&wd.finished);

      for(const auto &thread_id : read_threads)
        pthread_cancel(thread_id);

      for(const auto &thread_id : read_threads)
        pthread_join(thread_id,NULL);
    }

  sem_destroy(&wd.finished);

  return err;
}

#include "thread_pool.hpp"

#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"

#include "fuse_msgbuf.hpp"
#include "fuse_ll.hpp"

#include <errno.h>
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

int
fuse_session_loop_mt(struct fuse_session *se_,
                     const int            raw_read_thread_count_,
                     const int            raw_process_thread_count_)
{
  int err;
  int read_thread_count;
  int process_thread_count;
  fuse_worker_data_t wd = {0};
  std::vector<pthread_t> threads;

  read_thread_count    = raw_read_thread_count_;
  process_thread_count = raw_process_thread_count_;
  ::calculate_thread_counts(&read_thread_count,&process_thread_count);

  if(process_thread_count > 0)
    {
      wd.tp = std::make_shared<ThreadPool>(process_thread_count);
      wd.msgbuf_processor = process_msgbuf_async;
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
      threads.push_back(thread_id);
    }

  if(!err)
    {
      /* sem_wait() is interruptible */
      while(!fuse_session_exited(se_))
        sem_wait(&wd.finished);

      for(const auto &thread_id : threads)
        pthread_cancel(thread_id);

      for(const auto &thread_id : threads)
        pthread_join(thread_id,NULL);
    }

  sem_destroy(&wd.finished);

  return err;
}

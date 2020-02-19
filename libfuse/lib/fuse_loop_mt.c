/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "fuse_chan.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/* Environment var controlling the thread stack size */
#define ENVNAME_THREAD_STACK "FUSE_THREAD_STACK"

struct fuse_worker
{
  struct fuse_worker *prev;
  struct fuse_worker *next;
  struct fuse_mt *mt;
  pthread_t thread_id;
  fuse_chan_t *ch;
};

struct fuse_mt
{
  struct fuse_session *se;
  struct fuse_worker main;
  sem_t finish;
  int exit;
  int error;
};

static
void
list_add_worker(struct fuse_worker *w,
                struct fuse_worker *next)
{
  struct fuse_worker *prev = next->prev;
  w->next = next;
  w->prev = prev;
  prev->next = w;
  next->prev = w;
}

static
void
list_del_worker(struct fuse_worker *w)
{
  struct fuse_worker *prev = w->prev;
  struct fuse_worker *next = w->next;
  prev->next = next;
  next->prev = prev;
}

static int fuse_loop_start_thread(struct fuse_mt *mt);

static
void*
fuse_do_work(void *data)
{
  struct fuse_mt *mt;
  fuse_chan_t *ch;
  struct fuse_worker *w;

  w  = (struct fuse_worker*)data;
  mt = w->mt;
  ch = w->ch;
  while(!fuse_session_exited(mt->se))
    {
      int res;
      struct fuse_buf fbuf = { .mem = ch->buf, .size = ch->bufsize };

      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
      res = fuse_ll_receive_buf(mt->se,&fbuf,ch);
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      if(res == -EINTR)
        continue;
      if(res == 0)
        break;
      if(res < 0)
        {
          fuse_session_exit(mt->se);
          mt->error = -1;
          break;
        }

      if(mt->exit)
        return NULL;

      fuse_ll_process_buf(mt->se->data,&fbuf,ch);
    }

  sem_post(&mt->finish);

  return NULL;
}

int
fuse_start_thread(pthread_t *thread_id,
                  void *(*func)(void *),
                  void *arg)
{
  int res;
  sigset_t oldset;
  sigset_t newset;
  pthread_attr_t attr;
  char *stack_size;

  /* Override default stack size */
  pthread_attr_init(&attr);
  stack_size = getenv(ENVNAME_THREAD_STACK);
  if(stack_size && pthread_attr_setstacksize(&attr,atoi(stack_size)))
    fprintf(stderr, "fuse: invalid stack size: %s\n", stack_size);

  /* Disallow signal reception in worker threads */
  sigemptyset(&newset);
  sigaddset(&newset, SIGTERM);
  sigaddset(&newset, SIGINT);
  sigaddset(&newset, SIGHUP);
  sigaddset(&newset, SIGQUIT);
  pthread_sigmask(SIG_BLOCK, &newset, &oldset);
  res = pthread_create(thread_id, &attr, func, arg);
  pthread_sigmask(SIG_SETMASK, &oldset, NULL);
  pthread_attr_destroy(&attr);

  if(res != 0)
    {
      fprintf(stderr, "fuse: error creating thread: %s\n",
              strerror(res));
      return -1;
    }

  return 0;
}

static
int
fuse_loop_start_thread(struct fuse_mt *mt)
{
  int res;
  struct fuse_worker *w;

  w = malloc(sizeof(struct fuse_worker));
  if(!w)
    {
      fprintf(stderr, "fuse: failed to allocate worker structure\n");
      return -1;
    }

  memset(w,0,sizeof(struct fuse_worker));

  w->mt = mt;
  w->ch = fuse_chan_new(mt->se->devfuse_fd,FUSE_CHAN_SPLICE_READ|FUSE_CHAN_SPLICE_WRITE);
  if(w->ch == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate fuse channel\n");
      free(w);
      return -1;
    }

  res = fuse_start_thread(&w->thread_id, fuse_do_work, w);
  if(res == -1)
    {
      free(w);
      return -1;
    }

  list_add_worker(w,&mt->main);

  return 0;
}

static void fuse_join_worker(struct fuse_worker *w)
{
  pthread_join(w->thread_id, NULL);
  list_del_worker(w);
  fuse_chan_destroy(w->ch);
  free(w);
}

static int number_of_threads(void)
{
#ifdef _SC_NPROCESSORS_ONLN
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif

  return 4;
}

int fuse_session_loop_mt(struct fuse_session *se,
                         const int            threads_)
{
  int i;
  int err;
  int threads;
  struct fuse_mt mt;
  struct fuse_worker *w;

  memset(&mt, 0, sizeof(struct fuse_mt));
  mt.se = se;
  mt.error = 0;
  mt.main.thread_id = pthread_self();
  mt.main.prev = mt.main.next = &mt.main;
  sem_init(&mt.finish, 0, 0);

  threads = ((threads_ > 0) ? threads_ : number_of_threads());
  if(threads_ < 0)
    threads /= -threads_;
  if(threads == 0)
    threads = 1;

  err = 0;
  for(i = 0; (i < threads) && !err; i++)
    err = fuse_loop_start_thread(&mt);

  if(!err)
    {
      /* sem_wait() is interruptible */
      while(!fuse_session_exited(se))
        sem_wait(&mt.finish);

      for(w = mt.main.next; w != &mt.main; w = w->next)
        pthread_cancel(w->thread_id);
      mt.exit = 1;

      while(mt.main.next != &mt.main)
        fuse_join_worker(mt.main.next);

      err = mt.error;
    }

  sem_destroy(&mt.finish);
  fuse_session_reset(se);
  return err;
}

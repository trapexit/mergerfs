/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include "bounded_queue.hpp"
#include "invocable.h"

#include "mutex.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <pthread.h>
#include <syslog.h>


struct ThreadPoolTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
  static const int MAX_SEMA_SPINS = 1;
};


class ThreadPool
{
public:
  using PToken = moodycamel::ProducerToken;
  using CToken = moodycamel::ConsumerToken;

private:
  using Func  = ofats::any_invocable<void()>;
  using Queue = BoundedQueue<Func,ThreadPoolTraits>;

public:
  explicit
  ThreadPool(const unsigned thread_count_ = std::thread::hardware_concurrency(),
             const unsigned max_queue_depth_ = std::thread::hardware_concurrency() * 2,
             const std::string &name_ = {});
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

private:
  static void *start_routine(void *arg_);

public:
  int add_thread(const std::string &name = {});
  int remove_thread();
  int set_threads(const std::size_t count);

public:
  template<typename FuncType>
  void
  enqueue_work(ThreadPool::PToken  &ptok_,
               FuncType           &&func_);

  template<typename FuncType>
  void
  enqueue_work(FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work(ThreadPool::PToken &ptok_,
                   FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work(FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work_for(ThreadPool::PToken  &ptok_,
                       std::int64_t         timeout_usecs_,
                       FuncType           &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work_for(std::int64_t  timeout_usecs_,
                       FuncType    &&func_);

  template<typename FuncType>
  [[nodiscard]]
  std::future<std::invoke_result_t<FuncType>>
  enqueue_task(FuncType&& func_);

public:
  std::vector<pthread_t> threads() const;
  ThreadPool::PToken ptoken();

private:
  Queue _queue;

private:
  std::string const      _name;
  std::vector<pthread_t> _threads;
  mutable Mutex          _threads_mutex;
};



inline
ThreadPool::ThreadPool(const unsigned     thread_count_,
                       const unsigned     max_queue_depth_,
                       const std::string &name_)
  : _queue(max_queue_depth_),
    _name(name_)
{
  sigset_t oldset;
  sigset_t newset;

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);

  _threads.reserve(thread_count_);
  for(std::size_t i = 0; i < thread_count_; ++i)
    {
      int rv;
      pthread_t t;

      rv = pthread_create(&t,NULL,ThreadPool::start_routine,this);
      if(rv != 0)
        {
          syslog(LOG_WARNING,
                 "threadpool (%s): error spawning thread - %d (%s)",
                 _name.c_str(),
                 rv,
                 strerror(rv));
          continue;
        }

      if(not _name.empty())
        {
          std::string name = _name.substr(0, 15);
          pthread_setname_np(t, name.c_str());
        }

      _threads.push_back(t);
    }

  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(_threads.empty())
    throw std::runtime_error("threadpool: failed to spawn any threads");

  syslog(LOG_DEBUG,
         "threadpool (%s): spawned %zu threads w/ max queue depth %u",
         _name.c_str(),
         _threads.size(),
         max_queue_depth_);
}


inline
ThreadPool::~ThreadPool()
{
  std::vector<pthread_t> threads;

  {
    mutex_lockguard(_threads_mutex);
    threads = _threads;
  }

  syslog(LOG_DEBUG,
         "threadpool (%s): destroying %zu threads",
         _name.c_str(),
         threads.size());

  for(auto t : threads)
    pthread_cancel(t);
  for(auto t : threads)
    pthread_join(t,NULL);
}

// Threads purposefully do not restore default signal handling.
inline
void*
ThreadPool::start_routine(void *arg_)
{
  ThreadPool *btp = static_cast<ThreadPool*>(arg_);

  ThreadPool::Func func;
  ThreadPool::Queue &q = btp->_queue;
  ThreadPool::CToken ctok(btp->_queue.make_ctoken());

  while(true)
    {
      q.wait_dequeue(ctok,func);

      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);

      try
        {
          func();
        }
#if defined(__GLIBCXX__)
      catch(const __cxxabiv1::__forced_unwind&)
        {
          throw;
        }
#endif
      catch(const std::exception &e)
        {
          syslog(LOG_CRIT,
                 "threadpool (%s): uncaught exception caught by worker - %s",
                 btp->_name.c_str(),
                 e.what());
        }
      catch(...)
        {
          syslog(LOG_CRIT,
                 "threadpool (%s): uncaught non-standard exception caught by worker",
                 btp->_name.c_str());

        }

      // force destruction to release resources
      func = {};

      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    }

  return nullptr;
}

inline
int
ThreadPool::add_thread(const std::string &name_)
{
  int rv;
  pthread_t t;
  sigset_t oldset;
  sigset_t newset;
  std::string name;

  name = (name_.empty() ? _name : name_);

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);
  rv = pthread_create(&t,NULL,ThreadPool::start_routine,this);
  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(rv != 0)
    {
      syslog(LOG_WARNING,
             "threadpool (%s): error spawning thread - %d (%s)",
             _name.c_str(),
             rv,
             strerror(rv));
      return -rv;
    }

  if(not name.empty())
    {
      std::string n = name.substr(0, 15);
      pthread_setname_np(t,n.c_str());
    }

  {
    mutex_lockguard(_threads_mutex);
    _threads.push_back(t);
  }

  return 0;
}

inline
int
ThreadPool::remove_thread()
{
  {
    mutex_lockguard(_threads_mutex);
    if(_threads.size() <= 1)
      return -EINVAL;
  }

  auto promise = std::make_shared<std::promise<pthread_t>>();

  auto func =
    [this,promise]()
    {
      pthread_t t;

      t = pthread_self();

      {
        mutex_lockguard(_threads_mutex);

        auto it = std::find(_threads.begin(),
                            _threads.end(),
                            t);
        if(it != _threads.end())
          _threads.erase(it);
      }

      promise->set_value(t);

      pthread_exit(NULL);
    };

  _queue.enqueue_unbounded(std::move(func));

  pthread_join(promise->get_future().get(),NULL);

  return 0;
}

inline
int
ThreadPool::set_threads(const std::size_t count_)
{
  if(count_ == 0)
    return -EINVAL;

  std::size_t current;

  {
    mutex_lockguard(_threads_mutex);
    current = _threads.size();
  }

  for(std::size_t i = current; i < count_; ++i)
    add_thread();
  for(std::size_t i = count_; i < current; ++i)
    remove_thread();

  return 0;
}

template<typename FuncType>
inline
void
ThreadPool::enqueue_work(ThreadPool::PToken  &ptok_,
                         FuncType           &&func_)
{
  _queue.enqueue(ptok_,
                 std::forward<FuncType>(func_));
}

template<typename FuncType>
inline
void
ThreadPool::enqueue_work(FuncType &&func_)
{
  _queue.enqueue(std::forward<FuncType>(func_));
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work(ThreadPool::PToken &ptok_,
                             FuncType &&func_)
{
  return _queue.try_enqueue(ptok_,
                            std::forward<FuncType>(func_));
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work(FuncType &&func_)
{
  return _queue.try_enqueue(std::forward<FuncType>(func_));
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work_for(ThreadPool::PToken  &ptok_,
                                 std::int64_t         timeout_usecs_,
                                 FuncType           &&func_)
{
  return _queue.try_enqueue_for(ptok_,
                                timeout_usecs_,
                                std::forward<FuncType>(func_));
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work_for(std::int64_t  timeout_usecs_,
                                 FuncType    &&func_)
{
  return _queue.try_enqueue_for(timeout_usecs_,
                                std::forward<FuncType>(func_));
}


template<typename FuncType>
[[nodiscard]]
inline
std::future<std::invoke_result_t<FuncType>>
ThreadPool::enqueue_task(FuncType&& func_)
{
  using TaskReturnType = std::invoke_result_t<FuncType>;
  using Promise        = std::promise<TaskReturnType>;

  Promise promise;
  auto    future = promise.get_future();

  auto work =
    [promise_ = std::move(promise),
     func_    = std::forward<FuncType>(func_)]() mutable
    {
      try
        {
          if constexpr (std::is_void_v<TaskReturnType>)
            {
              func_();
              promise_.set_value();
            }
          else
            {
              promise_.set_value(func_());
            }
        }
      catch(...)
        {
          promise_.set_exception(std::current_exception());
        }
    };

  _queue.enqueue(std::move(work));

  return future;
}


inline
std::vector<pthread_t>
ThreadPool::threads() const
{
  mutex_lockguard(_threads_mutex);

  return _threads;
}


inline
ThreadPool::PToken
ThreadPool::ptoken()
{
  return _queue.make_ptoken();
}

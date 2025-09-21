#pragma once

#include "moodycamel/blockingconcurrentqueue.h"
#include "moodycamel/lightweightsemaphore.h"
#include "invocable.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include <pthread.h>
#include <syslog.h>

#define SEM
//#undef SEM

#ifdef SEM
#define SEMA_WAIT(S) (S.wait())
#define SEMA_SIGNAL(S) (S.signal())
#else
#define SEMA_WAIT(S)
#define SEMA_SIGNAL(S)
#endif


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
  using Queue = moodycamel::BlockingConcurrentQueue<Func,ThreadPoolTraits>;

public:
  explicit
  ThreadPool(const unsigned thread_count_    = std::thread::hardware_concurrency(),
             const unsigned max_queue_depth_ = std::thread::hardware_concurrency(),
             std::string const name_         = {});
  ~ThreadPool();

private:
  static void *start_routine(void *arg_);

public:
  int add_thread(std::string const name = {});
  int remove_thread(void);
  int set_threads(const std::size_t count);

  void shutdown(void);

public:
  template<typename FuncType>
  void
  enqueue_work(ThreadPool::PToken  &ptok_,
               FuncType           &&func_);

  template<typename FuncType>
  void
  enqueue_work(FuncType &&func_);

  template<typename FuncType>
  [[nodiscard]]
  std::future<typename std::result_of<FuncType()>::type>
  enqueue_task(FuncType&& func_);

public:
  std::vector<pthread_t> threads() const;
  ThreadPool::PToken ptoken();

private:
  Queue _queue;
  moodycamel::LightweightSemaphore _sema;

private:
  std::string const      _name;
  std::vector<pthread_t> _threads;
  mutable std::mutex     _threads_mutex;
};



inline
ThreadPool::ThreadPool(const unsigned    thread_count_,
                       const unsigned    max_queue_depth_,
                       const std::string name_)
  : _queue(),
    _sema(max_queue_depth_),
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

      if(!_name.empty())
        pthread_setname_np(t,_name.c_str());

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
  syslog(LOG_DEBUG,
         "threadpool (%s): destroying %zu threads",
         _name.c_str(),
         _threads.size());

  for(auto t : _threads)
    pthread_cancel(t);
  for(auto t : _threads)
    pthread_join(t,NULL);
}


inline
void*
ThreadPool::start_routine(void *arg_)
{
  ThreadPool *btp = static_cast<ThreadPool*>(arg_);

  bool done;
  ThreadPool::Func func;
  ThreadPool::Queue &q = btp->_queue;
  moodycamel::LightweightSemaphore &sema = btp->_sema;
  ThreadPool::CToken ctok(btp->_queue);

  done = false;
  while(!done)
    {
      q.wait_dequeue(ctok,func);

      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);

      try
        {
          func();
        }
      catch(std::exception &e)
        {
          done = true;
        }

      SEMA_SIGNAL(sema);

      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    }

  return nullptr;
}



inline
int
ThreadPool::add_thread(const std::string name_)
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

  if(!name.empty())
    pthread_setname_np(t,name.c_str());

  {
    std::lock_guard<std::mutex> lg(_threads_mutex);
    _threads.push_back(t);
  }

  return 0;
}


inline
int
ThreadPool::remove_thread(void)
{
  {
    std::lock_guard<std::mutex> lg(_threads_mutex);
    if(_threads.size() <= 1)
      return -EINVAL;
  }

  std::promise<pthread_t> promise;
  auto func =
    [this,&promise]()
    {
      pthread_t t;

      t = pthread_self();
      promise.set_value(t);

      {
        std::lock_guard<std::mutex> lg(_threads_mutex);

        for(auto i = _threads.begin(); i != _threads.end(); ++i)
          {
            if(*i != t)
              continue;

            _threads.erase(i);
            break;
          }
      }

      pthread_exit(NULL);
    };

  enqueue_work(std::move(func));
  pthread_join(promise.get_future().get(),NULL);

  return 0;
}


inline
int
ThreadPool::set_threads(std::size_t const count_)
{
  int diff;
  {
    std::lock_guard<std::mutex> lg(_threads_mutex);

    diff = ((int)count_ - (int)_threads.size());
  }

  for(auto i = diff; i > 0; --i)
    add_thread();
  for(auto i = diff; i < 0; ++i)
    remove_thread();

  return diff;
}


inline
void
ThreadPool::shutdown(void)
{
  std::lock_guard<std::mutex> lg(_threads_mutex);

  for(pthread_t tid : _threads)
    pthread_cancel(tid);
  for(pthread_t tid : _threads)
    pthread_join(tid,NULL);

  _threads.clear();
}


template<typename FuncType>
inline
void
ThreadPool::enqueue_work(ThreadPool::PToken  &ptok_,
                         FuncType           &&func_)
{
  SEMA_WAIT(_sema);
  _queue.enqueue(ptok_,
                 std::forward<FuncType>(func_));
}


template<typename FuncType>
inline
void
ThreadPool::enqueue_work(FuncType &&func_)
{
  SEMA_WAIT(_sema);
  _queue.enqueue(std::forward<FuncType>(func_));
}


template<typename FuncType>
[[nodiscard]]
inline
std::future<typename std::result_of<FuncType()>::type>
ThreadPool::enqueue_task(FuncType&& func_)
{
  using TaskReturnType = typename std::result_of<FuncType()>::type;
  using Promise        = std::promise<TaskReturnType>;

  Promise promise;
  auto    future = promise.get_future();

  auto work =
    [promise_ = std::move(promise),
     func_    = std::forward<FuncType>(func_)]() mutable
    {
      try
        {
          auto rv = func_();
          promise_.set_value(std::move(rv));
        }
      catch(...)
        {
          promise_.set_exception(std::current_exception());
        }
    };

  SEMA_WAIT(_sema);
  _queue.enqueue(std::move(work));

  return future;
}


inline
std::vector<pthread_t>
ThreadPool::threads() const
{
  std::lock_guard<std::mutex> lg(_threads_mutex);

  return _threads;
}


inline
ThreadPool::PToken
ThreadPool::ptoken()
{
  return ThreadPool::PToken(_queue);
}

#pragma once

#include "moodycamel/blockingconcurrentqueue.h"

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

#include <syslog.h>


struct ThreadPoolTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
  static const int MAX_SEMA_SPINS = 1;
};


class ThreadPool
{
private:
  using Func  = std::function<void(void)>;
  using Queue = moodycamel::BlockingConcurrentQueue<Func,ThreadPoolTraits>;

public:
  explicit
  ThreadPool(unsigned const thread_count_    = std::thread::hardware_concurrency(),
             unsigned const max_queue_depth_ = std::thread::hardware_concurrency(),
             std::string const name_         = {})
    : _queue(),
      _queue_depth(0),
      _max_queue_depth(std::max(thread_count_,max_queue_depth_)),
      _name(name_)
  {
    syslog(LOG_DEBUG,
           "threadpool (%s): spawning %u threads w/ max queue depth %u%s",
           _name.c_str(),
           thread_count_,
           _max_queue_depth,
           ((_max_queue_depth != max_queue_depth_) ? " (adjusted)" : ""));

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
  }

  ~ThreadPool()
  {
    syslog(LOG_DEBUG,
           "threadpool (%s): destroying %lu threads",
           _name.c_str(),
           _threads.size());

    auto func = []() { pthread_exit(NULL); };
    for(std::size_t i = 0; i < _threads.size(); i++)
      _queue.enqueue(func);

    for(auto t : _threads)
      pthread_cancel(t);

    for(auto t : _threads)
      pthread_join(t,NULL);
  }

private:
  static
  void*
  start_routine(void *arg_)
  {
    ThreadPool *btp = static_cast<ThreadPool*>(arg_);
    ThreadPool::Func func;
    ThreadPool::Queue &q = btp->_queue;
    std::atomic<unsigned> &queue_depth = btp->_queue_depth;
    moodycamel::ConsumerToken ctok(btp->_queue);

    while(true)
      {
        q.wait_dequeue(ctok,func);

        func();

        queue_depth.fetch_sub(1,std::memory_order_release);
      }

    return NULL;
  }

public:
  int
  add_thread(std::string const name_ = {})
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

    syslog(LOG_DEBUG,
           "threadpool (%s): 1 thread added named '%s'",
           _name.c_str(),
           name.c_str());

    return 0;
  }

  int
  remove_thread(void)
  {
    {
      std::lock_guard<std::mutex> lg(_threads_mutex);
      if(_threads.size() <= 1)
        return -EINVAL;
    }

    std::promise<pthread_t> promise;
    auto func = [&]()
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

      syslog(LOG_DEBUG,
             "threadpool (%s): 1 thread removed",
             _name.c_str());

      pthread_exit(NULL);
    };

    enqueue_work(func);
    pthread_join(promise.get_future().get(),NULL);

    return 0;
  }

  int
  set_threads(std::size_t const count_)
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

public:
  template<typename FuncType>
  void
  enqueue_work(moodycamel::ProducerToken  &ptok_,
               FuncType                  &&f_)
  {
    timespec ts = {0,1000};
    for(unsigned i = 0; i < 1000000; i++)
      {
        if(_queue_depth.load(std::memory_order_acquire) < _max_queue_depth)
          break;
        ::nanosleep(&ts,NULL);
      }

    _queue.enqueue(ptok_,f_);
    _queue_depth.fetch_add(1,std::memory_order_release);
  }

  template<typename FuncType>
  void
  enqueue_work(FuncType &&f_)
  {
    timespec ts = {0,1000};
    for(unsigned i = 0; i < 1000000; i++)
      {
        if(_queue_depth.load(std::memory_order_acquire) < _max_queue_depth)
          break;
        ::nanosleep(&ts,NULL);
      }

    _queue.enqueue(f_);
    _queue_depth.fetch_add(1,std::memory_order_release);
  }

  template<typename FuncType>
  [[nodiscard]]
  std::future<typename std::result_of<FuncType()>::type>
  enqueue_task(FuncType&& f_)
  {
    using TaskReturnType = typename std::result_of<FuncType()>::type;
    using Promise        = std::promise<TaskReturnType>;

    auto promise = std::make_shared<Promise>();
    auto future  = promise->get_future();

    auto work = [=]()
    {
      auto rv = f_();
      promise->set_value(rv);
    };

    timespec ts = {0,1000};
    for(unsigned i = 0; i < 1000000; i++)
      {
        if(_queue_depth.load(std::memory_order_acquire) < _max_queue_depth)
          break;
        ::nanosleep(&ts,NULL);
      }

    _queue.enqueue(work);
    _queue_depth.fetch_add(1,std::memory_order_release);

    return future;
  }

public:
  std::vector<pthread_t>
  threads() const
  {
    std::lock_guard<std::mutex> lg(_threads_mutex);

    return _threads;
  }

  moodycamel::ProducerToken
  ptoken()
  {
    return moodycamel::ProducerToken(_queue);
  }

private:
  Queue _queue;
  std::atomic<unsigned> _queue_depth;
  unsigned const        _max_queue_depth;

private:
  std::string const      _name;
  std::vector<pthread_t> _threads;
  mutable std::mutex     _threads_mutex;
};

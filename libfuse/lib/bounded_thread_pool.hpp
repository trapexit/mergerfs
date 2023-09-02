#pragma once

#include "moodycamel/blockingconcurrentqueue.h"
#include "syslog.h"

#include <atomic>
#include <csignal>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


struct BoundedThreadPoolTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
  static const int MAX_SEMA_SPINS = 1;
};


class BoundedThreadPool
{
private:
  using Func  = std::function<void(void)>;
  using Queue = moodycamel::BlockingConcurrentQueue<Func,BoundedThreadPoolTraits>;

public:
  explicit
  BoundedThreadPool(std::size_t const thread_count_ = std::thread::hardware_concurrency(),
                    std::size_t const queue_depth_  = 1,
                    std::string const name_         = {})
    : _queue(queue_depth_,thread_count_,thread_count_),
      _done(false),
      _name(name_)
  {
    syslog_debug("threadpool: spawning %zu threads of queue depth %zu named '%s'",
                 thread_count_,
                 queue_depth_,
                 _name.c_str());

    sigset_t oldset;
    sigset_t newset;

    sigfillset(&newset);
    pthread_sigmask(SIG_BLOCK,&newset,&oldset);

    _threads.reserve(thread_count_);
    for(std::size_t i = 0; i < thread_count_; ++i)
      {
        int rv;
        pthread_t t;

        rv = pthread_create(&t,NULL,BoundedThreadPool::start_routine,this);
        if(rv != 0)
          {
            syslog_warning("threadpool: error spawning thread - %d (%s)",
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

  ~BoundedThreadPool()
  {
    syslog_debug("threadpool: destroying %zu threads named '%s'",
                 _threads.size(),
                 _name.c_str());

    _done.store(true,std::memory_order_relaxed);

    for(auto t : _threads)
      pthread_cancel(t);

    Func f;
    while(_queue.try_dequeue(f))
      continue;

    for(auto t : _threads)
      pthread_join(t,NULL);
  }

private:
  static
  void*
  start_routine(void *arg_)
  {
    BoundedThreadPool *btp = static_cast<BoundedThreadPool*>(arg_);
    BoundedThreadPool::Func func;
    std::atomic<bool> &done = btp->_done;
    BoundedThreadPool::Queue &q = btp->_queue;
    moodycamel::ConsumerToken ctok(btp->_queue);

    while(!done.load(std::memory_order_relaxed))
      {
        q.wait_dequeue(ctok,func);

        func();
      }

    return NULL;
  }

public:
  template<typename FuncType>
  void
  enqueue_work(moodycamel::ProducerToken  &ptok_,
               FuncType                  &&f_)
  {
    timespec ts = {0,10};
    while(true)
      {
        if(_queue.try_enqueue(ptok_,f_))
          return;
        ::nanosleep(&ts,NULL);
        ts.tv_nsec += 10;
      }
  }

  template<typename FuncType>
  void
  enqueue_work(FuncType &&f_)
  {
    timespec ts = {0,10};
    while(true)
      {
        if(_queue.try_enqueue(f_))
          return;
        ::nanosleep(&ts,NULL);
        ts.tv_nsec += 10;
      }
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
    auto work    = [=]()
    {
      auto rv = f_();
      promise->set_value(rv);
    };

    timespec ts = {0,10};
    while(true)
      {
        if(_queue.try_enqueue(work))
          break;
        ::nanosleep(&ts,NULL);
        ts.tv_nsec += 10;
      }

    return future;
  }

public:
  std::vector<pthread_t>
  threads() const
  {
    return _threads;
  }

public:
  Queue&
  queue()
  {
    return _queue;
  }

private:
  Queue _queue;

private:
  std::atomic<bool>      _done;
  std::string const      _name;
  std::vector<pthread_t> _threads;
};

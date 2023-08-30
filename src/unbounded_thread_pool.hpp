#pragma once

#include "moodycamel/blockingconcurrentqueue.h"
#include "syslog.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


struct UnboundedThreadPoolTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
  static const int MAX_SEMA_SPINS = 1;
};


class UnboundedThreadPool
{
private:
  using Func   = std::function<void(void)>;
  using Queue  = moodycamel::BlockingConcurrentQueue<Func,UnboundedThreadPoolTraits>;

public:
  explicit
  UnboundedThreadPool(std::size_t const thread_count_ = std::thread::hardware_concurrency(),
                      std::string const name_         = {})
    : _queue(thread_count_,thread_count_,thread_count_),
      _done(false),
      _name(name_)
  {
    syslog_debug("threadpool: spawning %zu threads named '%s'",
                 thread_count_,
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

        rv = pthread_create(&t,NULL,UnboundedThreadPool::start_routine,this);
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

  ~UnboundedThreadPool()
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
    UnboundedThreadPool *btp = static_cast<UnboundedThreadPool*>(arg_);
    UnboundedThreadPool::Func func;
    std::atomic<bool> &done = btp->_done;
    UnboundedThreadPool::Queue &q = btp->_queue;
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
    _queue.enqueue(ptok_,f_);
  }

  template<typename FuncType>
  void
  enqueue_work(FuncType &&f_)
  {
    _queue.enqueue(f_);
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

    _queue.enqueue(work);

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

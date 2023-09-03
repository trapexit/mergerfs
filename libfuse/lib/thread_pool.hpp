#pragma once

#include "moodycamel/blockingconcurrentqueue.h"
#include "syslog.h"

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
  ThreadPool(std::size_t const thread_count_ = std::thread::hardware_concurrency(),
             std::size_t const queue_depth_  = 1,
             std::string const name_         = {})
    : _queue(queue_depth_,thread_count_,thread_count_),
      _name(get_thread_name(name_))
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

        rv = pthread_create(&t,NULL,ThreadPool::start_routine,this);
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

  ~ThreadPool()
  {
    syslog_debug("threadpool: destroying %zu threads named '%s'",
                 _threads.size(),
                 _name.c_str());

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
  std::string
  get_thread_name(std::string const name_)
  {
    if(!name_.empty())
      return name_;

    char name[16];
    pthread_getname_np(pthread_self(),name,sizeof(name));

    return name;
  }

  static
  void*
  start_routine(void *arg_)
  {
    ThreadPool *btp = static_cast<ThreadPool*>(arg_);
    ThreadPool::Func func;
    ThreadPool::Queue &q = btp->_queue;
    moodycamel::ConsumerToken ctok(btp->_queue);

    while(true)
      {
        q.wait_dequeue(ctok,func);

        func();
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
        syslog_warning("threadpool: error spawning thread - %d (%s)",
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

    syslog_debug("threadpool: 1 thread added to pool '%s' named '%s'",
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

      char name[16];
      pthread_getname_np(t,name,sizeof(name));
      syslog_debug("threadpool: 1 thread removed from pool '%s' named '%s'",
                   _name.c_str(),
                   name);

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

    syslog_debug("diff: %d",diff);
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

private:
  std::string const      _name;
  std::vector<pthread_t> _threads;
  mutable std::mutex     _threads_mutex;
};

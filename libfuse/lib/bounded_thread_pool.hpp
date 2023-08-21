#pragma once

#include "bounded_queue.hpp"
#include "make_unique.hpp"

#include <atomic>
#include <csignal>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>


class BoundedThreadPool
{
private:
  using Proc   = std::function<void(void)>;
  using Queue  = BoundedQueue<Proc>;
  using Queues = std::vector<std::unique_ptr<Queue>>;

public:
  explicit
  BoundedThreadPool(std::size_t const thread_count_ = std::thread::hardware_concurrency(),
                    std::size_t const queue_depth_  = 1,
                    std::string const name_         = {})
    : _queues(),
      _count(thread_count_)
  {
    for(std::size_t i = 0; i < thread_count_; i++)
      _queues.emplace_back(std::make_unique<Queue>(queue_depth_));

    auto worker = [this](std::size_t i)
    {
      while(true)
        {
          Proc f;

          for(std::size_t n = 0; n < (_count * K); ++n)
            {
              if(_queues[(i + n) % _count]->pop(f))
                break;
            }

          if(!f && !_queues[i]->pop(f))
            break;

          f();
        }
    };

    sigset_t oldset;
    sigset_t newset;

    sigfillset(&newset);
    pthread_sigmask(SIG_BLOCK,&newset,&oldset);

    _threads.reserve(thread_count_);
    for(std::size_t i = 0; i < thread_count_; ++i)
      _threads.emplace_back(worker, i);
    if(!name_.empty())
      {
        for(auto &t : _threads)
          pthread_setname_np(t.native_handle(),name_.c_str());
      }

    pthread_sigmask(SIG_SETMASK,&oldset,NULL);
  }

  ~BoundedThreadPool()
  {
    for(auto &queue : _queues)
      queue->unblock();
    for(auto &thread : _threads)
      pthread_cancel(thread.native_handle());
    for(auto &thread : _threads)
      thread.join();
  }

  template<typename F>
  void
  enqueue_work(F&& f_)
  {
    auto i = _index.fetch_add(1,std::memory_order_relaxed);

    for(std::size_t n = 0; n < (_count * K); ++n)
      {
        if(_queues[(i + n) % _count]->push(f_))
          return;
      }

    _queues[i % _count]->push(std::move(f_));
  }

  template<typename F>
  [[nodiscard]]
  std::future<typename std::result_of<F()>::type>
  enqueue_task(F&& f_)
  {
    using TaskReturnType = typename std::result_of<F()>::type;
    using Promise        = std::promise<TaskReturnType>;

    auto i       = _index.fetch_add(1,std::memory_order_relaxed);
    auto promise = std::make_shared<Promise>();
    auto future  = promise->get_future();
    auto work    = [=]()
    {
      auto rv = f_();
      promise->set_value(rv);
    };

    for(std::size_t n = 0; n < (_count * K); ++n)
      {
        if(_queues[(i + n) % _count]->push(work))
          return future;
      }

    _queues[i % _count]->push(std::move(work));

    return future;
  }

public:
  std::vector<pthread_t>
  threads()
  {
    std::vector<pthread_t> rv;

    for(auto &thread : _threads)
      rv.push_back(thread.native_handle());

    return rv;
  }

private:
  Queues _queues;

private:
  std::vector<std::thread> _threads;

private:
  const std::size_t _count;
  std::atomic_uint _index;

  static const unsigned int K = 2;
};

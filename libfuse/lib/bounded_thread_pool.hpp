#pragma once

#include "bounded_queue.hpp"

#include <tuple>
#include <atomic>
#include <vector>
#include <thread>
#include <memory>
#include <future>
#include <utility>
#include <functional>
#include <type_traits>


class BoundedThreadPool
{
private:
  using Proc   = std::function<void(void)>;
  using Queue  = BoundedQueue<Proc>;
  using Queues = std::vector<std::shared_ptr<Queue>>;
  
public:
  explicit
  BoundedThreadPool(const std::size_t thread_count_ = std::thread::hardware_concurrency())
    : _queues(),
      _count(thread_count_)
  {
    printf("threads: %d\n",thread_count_);
    for(std::size_t i = 0; i < thread_count_; i++)
      _queues.emplace_back(std::make_shared<Queue>(1));
    
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

    _threads.reserve(thread_count_);
    for(std::size_t i = 0; i < thread_count_; ++i)
      _threads.emplace_back(worker, i);
  }

  ~BoundedThreadPool()
  {
    for(auto& queue : _queues)
      queue->unblock();
    for(auto& thread : _threads)
      thread.join();
  }

  template<typename F>
  void
  enqueue_work(F&& f_)
  {
    auto i = _index++;

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

    auto i       = _index++;
    auto promise = std::make_shared<Promise>();
    auto future  = promise->get_future();
    auto work    = [=]() {
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

#pragma once

#include "unbounded_queue.hpp"

#include <tuple>
#include <atomic>
#include <vector>
#include <thread>
#include <memory>
#include <future>
#include <utility>
#include <functional>
#include <type_traits>


class ThreadPool
{
public:
  explicit
  ThreadPool(const std::size_t thread_count_ = std::thread::hardware_concurrency())
    : _queues(thread_count_),
      _count(thread_count_)
  {
    auto worker = [this](std::size_t i)
    {
      while(true)
        {
          Proc f;

          for(std::size_t n = 0; n < (_count * K); ++n)
            {
              if(_queues[(i + n) % _count].try_pop(f))
                break;
            }

          if(!f && !_queues[i].pop(f))
            break;

          f();
        }
    };

    _threads.reserve(thread_count_);
    for(std::size_t i = 0; i < thread_count_; ++i)
      _threads.emplace_back(worker, i);
  }

  ~ThreadPool()
  {
    for(auto& queue : _queues)
      queue.unblock();
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
        if(_queues[(i + n) % _count].try_push(f_))
          return;
      }

    _queues[i % _count].push(std::move(f_));
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
        if(_queues[(i + n) % _count].try_push(work))
          return future;
      }

    _queues[i % _count].push(std::move(work));

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
  using Proc   = std::function<void(void)>;
  using Queue  = UnboundedQueue<Proc>;
  using Queues = std::vector<Queue>;
  Queues _queues;

private:
  std::vector<std::thread> _threads;

private:
  const std::size_t _count;
  std::atomic_uint  _index;

  static const unsigned int K = 2;
};

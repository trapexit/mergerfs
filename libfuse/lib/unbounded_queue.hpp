#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>


template<typename T>
class UnboundedQueue
{
public:
  explicit
  UnboundedQueue(bool block_ = true)
    : _block(block_)
  {
  }

  void
  push(const T& item_)
  {
    {
      std::lock_guard<std::mutex> guard(_queue_lock);
      _queue.push(item_);
    }
    _condition.notify_one();
  }

  void
  push(T&& item_)
  {
    {
      std::lock_guard<std::mutex> guard(_queue_lock);
      _queue.push(std::move(item_));
    }

    _condition.notify_one();
  }

  template<typename... Args>
  void
  emplace(Args&&... args_)
  {
    {
      std::lock_guard<std::mutex> guard(_queue_lock);
      _queue.emplace(std::forward<Args>(args_)...);
    }

    _condition.notify_one();
  }

  bool
  try_push(const T& item_)
  {
    {
      std::unique_lock<std::mutex> lock(_queue_lock, std::try_to_lock);
      if(!lock)
        return false;
      _queue.push(item_);
    }

    _condition.notify_one();

    return true;
  }

  bool
  try_push(T&& item_)
  {
    {
      std::unique_lock<std::mutex> lock(_queue_lock, std::try_to_lock);
      if(!lock)
        return false;
      _queue.push(std::move(item_));
    }

    _condition.notify_one();

    return true;
  }

  //TODO: push multiple T at once

  bool
  pop(T& item_)
  {
    std::unique_lock<std::mutex> guard(_queue_lock);

    _condition.wait(guard, [&]() { return !_queue.empty() || !_block; });
    if(_queue.empty())
      return false;

    item_ = std::move(_queue.front());
    _queue.pop();

    return true;
  }

  bool
  try_pop(T& item_)
  {
    std::unique_lock<std::mutex> lock(_queue_lock, std::try_to_lock);
    if(!lock || _queue.empty())
      return false;

    item_ = std::move(_queue.front());
    _queue.pop();

    return true;
  }

  std::size_t
  size() const
  {
    std::lock_guard<std::mutex> guard(_queue_lock);

    return _queue.size();
  }

  bool
  empty() const
  {
    std::lock_guard<std::mutex> guard(_queue_lock);

    return _queue.empty();
  }

  void
  block()
  {
    std::lock_guard<std::mutex> guard(_queue_lock);
    _block = true;
  }

  void
  unblock()
  {
    {
      std::lock_guard<std::mutex> guard(_queue_lock);
      _block = false;
    }

    _condition.notify_all();
  }

  bool
  blocking() const
  {
    std::lock_guard<std::mutex> guard(_queue_lock);

    return _block;
  }

private:
  mutable std::mutex _queue_lock;

private:
  bool _block;
  std::queue<T> _queue;
  std::condition_variable _condition;
};

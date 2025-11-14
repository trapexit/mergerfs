#pragma once

#include <memory>
#include <utility>

template<typename T>
class AtomicSharedPtr
{
private:
  std::shared_ptr<T> _ptr;

public:
  AtomicSharedPtr() = default;

  explicit
  AtomicSharedPtr(std::shared_ptr<T> ptr_)
    : _ptr(std::move(ptr_))
  {
  }

  template<typename... Args>
  explicit
  AtomicSharedPtr(Args&&... args)
    : _ptr(std::make_shared<T>(std::forward<Args>(args)...))
  {
  }

  AtomicSharedPtr(const AtomicSharedPtr&) = delete;
  AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

  AtomicSharedPtr(AtomicSharedPtr&& other_) noexcept
    : _ptr(std::move(other_._ptr))
  {
  }

  AtomicSharedPtr&
  operator=(AtomicSharedPtr&& other_) noexcept
  {
    if(this != &other_)
      _ptr = std::move(other_._ptr);

    return *this;
  }

  std::shared_ptr<T>
  load() const
  {
    return std::atomic_load(&_ptr);
  }

  void
  store(std::shared_ptr<T> desired_)
  {
    std::atomic_store(&_ptr, std::move(desired_));
  }

  std::shared_ptr<T>
  exchange(std::shared_ptr<T> desired_)
  {
    return std::atomic_exchange(&_ptr, std::move(desired_));
  }

  bool
  compare_exchange_strong(std::shared_ptr<T>& expected_,
                          std::shared_ptr<T>  desired_)
  {
    return std::atomic_compare_exchange_strong(&_ptr,
                                               &expected_,
                                               std::move(desired_));
  }

  bool
  compare_exchange_weak(std::shared_ptr<T>& expected_,
                        std::shared_ptr<T>  desired_)
  {
    return std::atomic_compare_exchange_weak(&_ptr,
                                             &expected_,
                                             std::move(desired_));
  }

  bool
  is_null() const
  {
    return !std::atomic_load(&_ptr);
  }

  void
  reset()
  {
    std::atomic_store(&_ptr, std::shared_ptr<T>());
  }

  T*
  get() const
  {
    return std::atomic_load(&_ptr).get();
  }

  template<typename... Args>
  auto
  operator()(Args&&... args) const ->
    decltype(std::declval<T>()(std::forward<Args>(args)...))
  {
    auto ptr = load();

    return (*ptr)(std::forward<Args>(args)...);
  }
};

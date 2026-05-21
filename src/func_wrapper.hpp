#pragma once

#include "fatal.hpp"
#include "tofrom_string.hpp"

#include <cerrno>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>

#include <cassert>

template<typename BaseType,
         typename FactoryType,
         typename ReturnType,
         typename... Args>
class FuncWrapper : public ToFromString
{
private:
  bool _initialized = false;
  mutable std::shared_mutex _mutex;
  std::shared_ptr<BaseType> _impl;

public:
  explicit FuncWrapper(const std::string &name_)
  {
    // Hard-coded defaults must always parse. Failure here is a build
    // error masquerading as a runtime bug, so check unconditionally
    // (assert alone is elided under NDEBUG).
    const int rv = from_string(name_);
    if(rv != 0)
      fatal::abort("FuncWrapper: hard-coded default '{}' is not a valid policy name",name_);
  }

public:
  ReturnType
  operator()(Args&&... args)
  {
    std::shared_ptr<BaseType> impl;

    {
      std::shared_lock<std::shared_mutex> lk(_mutex);
      impl = _impl;
    }
    if(!impl)
      fatal::abort("function policy impl is null");

    return (*impl)(std::forward<Args>(args)...);
  }

public:
  std::shared_ptr<BaseType>
  impl() const
  {
    std::shared_lock<std::shared_mutex> lk(_mutex);
    return _impl;
  }

public:
  std::string
  to_string() const
  {
    std::shared_ptr<BaseType> impl;
    {
      std::shared_lock<std::shared_mutex> lk(_mutex);
      impl = _impl;
    }
    if(!impl)
      return {};

    return std::string(impl->name());
  }

  int
  from_string(const std::string_view str_)
  {
    std::shared_ptr<BaseType> p;

    p = FactoryType::make(str_);
    if(!p)
      return -EINVAL;

    {
      std::unique_lock<std::shared_mutex> lk(_mutex);

      _impl = std::move(p);
    }

    return 0;
  }

public:
  void
  initialize()
  {
    std::unique_lock<std::shared_mutex> lk(_mutex);

    _initialized = true;
  }
};

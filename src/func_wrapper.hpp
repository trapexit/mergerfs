#pragma once

#include "atomicsharedptr.hpp"
#include "tofrom_string.hpp"

#include <string>
#include <string_view>

#include <cassert>

template<typename BaseType,
         typename FactoryType,
         typename ReturnType,
         typename... Args>
class FuncWrapper : public ToFromString
{
private:
  AtomicSharedPtr<BaseType> _impl;

public:
  explicit FuncWrapper(const std::string &name_)
  {
    _impl.store(FactoryType::make(name_));
    assert(not _impl.is_null());
  }

public:
  ReturnType
  operator()(Args&&... args)
  {
    return _impl(std::forward<Args>(args)...);
    //return _impl(args...);
  }

public:
  std::string
  to_string() const
  {
    return std::string{_impl.load()->name()};
  }

  int
  from_string(const std::string_view str_)
  {
    std::shared_ptr<BaseType> p;

    p = FactoryType::make(str_);
    if(!p)
      return -EINVAL;

    _impl.store(p);

    return 0;
  }
};

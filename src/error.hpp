#pragma once

#include "errno.hpp"

#include <optional>


struct Err
{
private:
  std::optional<int> _err;

public:
  Err()
  {
  }

  operator int()
  {
    return (_err.has_value() ? _err.value() : -ENOENT);
  }

  Err&
  operator=(int v_)
  {
    if(!_err.has_value())
      _err = v_;
    else if(v_ >= 0)
      _err = v_;

    return *this;
  }

  bool
  operator==(int v_)
  {
    if(_err.has_value())
      return (_err.value() == v_);
    return false;
  }
};

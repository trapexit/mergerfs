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

  operator int() const
  {
    return (_err.has_value() ? _err.value() : 0);
  }

  // A rough heuristic for prioritizing errors. Success always
  // overrides failures. ENOENT is lowest errno priority, then EROFS
  // (which can be set due to branch mode = RO) then any other error.
  Err&
  operator=(int new_err_)
  {
    if(!_err.has_value())
      _err = ((new_err_ >= 0) ? 0 : new_err_);
    else if(new_err_ >= 0)
      _err = 0;
    else if(_err == -ENOENT)
      _err = new_err_;
    else if((_err == -EROFS) && (new_err_ != -ENOENT))
      _err = new_err_;

    return *this;
  }

  bool
  operator==(int other_)
  {
    if(_err.has_value())
      return (_err.value() == other_);
    return false;
  }
};

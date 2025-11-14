#pragma once

#include "config.hpp"
#include "tofrom_string.hpp"

class ConfigGetAttrStatx : public ToFromString
{
private:
  Func2::GetAttr &_getattr;
  Func2::Statx   &_statx;

public:
  ConfigGetAttrStatx() = delete;
  ConfigGetAttrStatx(Func2::GetAttr &getattr_,
                     Func2::Statx   &statx_)
    : _getattr(getattr_),
      _statx(statx_)
  {
  }

  std::string
  to_string() const
  {
    return _getattr.to_string();
  }

  int
  from_string(const std::string_view str_)
  {
    int rv;

    rv = _getattr.from_string(str_);
    if(rv < 0)
      return rv;

    rv = _statx.from_string(str_);
    assert(rv == 0);

    return rv;
  }
};

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
    std::string g = _getattr.to_string();
    std::string s = _statx.to_string();

    if(g == s)
      return g;

    return g + "," + s;
  }

  int
  from_string(const std::string_view str_)
  {
    int rv;

    if(!Func2::GetAttrFactory::valid(std::string(str_)))
      return -EINVAL;
    if(!Func2::StatxFactory::valid(std::string(str_)))
      return -EINVAL;

    rv = _getattr.from_string(str_);
    if(rv < 0)
      return rv;

    rv = _statx.from_string(str_);
    if(rv < 0)
      return rv;

    return 0;
  }
};

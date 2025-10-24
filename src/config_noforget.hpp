#pragma once

#include "tofrom_string.hpp"
#include "from_string.hpp"

#include "fuse_cfg.hpp"


class CfgNoforget : public ToFromString
{
public:
  int
  from_string(const std::string_view s_)
  {
    int rv;
    bool b = false;

    rv = str::from(s_,&b);
    if((b == true) || s_.empty())
      {
        fuse_cfg.remember_nodes = -1;
        return 0;
      }

    if(rv)
      return rv;

    fuse_cfg.remember_nodes = 0;

    return 0;
  }

  std::string
  to_string() const
  {
    if(fuse_cfg.remember_nodes == -1)
      return "true";
    return "false";
  }
};

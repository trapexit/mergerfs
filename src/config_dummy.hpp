#pragma once

#include "tofrom_string.hpp"

class CfgDummy : public ToFromString
{
public:
  CfgDummy()
  {
    display = false;
  }

  std::string
  to_string() const
  {
    return {};
  }

  int
  from_string(const std::string_view)
  {
    return 0;
  }
};

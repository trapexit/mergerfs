#pragma once

#include "tofrom_string.hpp"

class ProxyIOPrio : public ToFromString
{
public:
  ProxyIOPrio(const bool);

public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

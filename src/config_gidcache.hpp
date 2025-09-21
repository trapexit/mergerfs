#pragma once

#include "tofrom_string.hpp"

class GIDCacheExpireTimeout : public ToFromString
{
public:
  GIDCacheExpireTimeout(const int i = 60 * 60);
  GIDCacheExpireTimeout(const std::string &);

public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

class GIDCacheRemoveTimeout : public ToFromString
{
public:
  GIDCacheRemoveTimeout(const int = 60 * 60 * 12);
  GIDCacheRemoveTimeout(const std::string &);

public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

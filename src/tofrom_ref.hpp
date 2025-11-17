#pragma once

#include "to_string.hpp"
#include "from_string.hpp"
#include "tofrom_string.hpp"


template<typename T>
class TFSRef : public ToFromString
{
public:
  int
  from_string(const std::string_view s_) final
  {
    return str::from(s_,&_data);
  }

  std::string
  to_string(void) const final
  {
    return str::to(_data);
  }

public:
  TFSRef(T &data_)
    : _data(data_)
  {
  }

private:
  T &_data;
};

#pragma once

#include "func_listxattr_base.hpp"

namespace Func2
{
  class ListxattrAll : public ListxattrBase
  {
  public:
    ListxattrAll() {}
    ~ListxattrAll() {}

  public:
    std::string_view name() const;

  public:
    ssize_t operator()(const Branches &branches,
                       const fs::path &fusepath,
                       char           *list,
                       const size_t    size);
  };
}

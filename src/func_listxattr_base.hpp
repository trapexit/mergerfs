#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

namespace Func2
{
  class ListxattrBase
  {
  public:
    ListxattrBase() {}
    ~ListxattrBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual ssize_t operator()(const Branches &branches,
                               const fs::path &fusepath,
                               char           *list,
                               const size_t    size) = 0;
  };
}

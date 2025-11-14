#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

namespace Func2
{
  class SetxattrBase
  {
  public:
    SetxattrBase() {}
    ~SetxattrBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           const char     *attrname,
                           const char     *attrval,
                           size_t          attrvalsize,
                           int             flags) = 0;
  };
}

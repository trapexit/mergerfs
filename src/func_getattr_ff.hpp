#pragma once

#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrFF : public GetAttrBase
  {
  public:
    GetAttrFF() {}
    ~GetAttrFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   struct stat    *st,
                   const FollowSymlinksEnum follow_symlinks,
                   const s64 symlinkify_timeout);
  };
}

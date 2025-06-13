#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrCombine : public GetAttrBase
  {
  public:
    GetAttrCombine() {}
    ~GetAttrCombine() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::Path &fusepath,
                   struct stat    *st,
                   const bool      follow_symlinks,
                   const uint64_t  symlinkify_timeout);
  };
}

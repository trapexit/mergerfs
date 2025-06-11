#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrNewest : public GetAttrBase
  {
  public:
    GetAttrNewest() {}
    ~GetAttrNewest() {}

  public:
    int operator()(const Branches &branches,
                   const fs::Path &fusepath,
                   struct stat *st,
                   fuse_timeouts_t *timeout);
  };
}

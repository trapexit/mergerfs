#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrCombine : public GetAttrBase
  {
  public:
    GetAttrCombine() {}
    ~GetAttrCombine() {}

  public:
    int operator()(const Branches &branches,
                   const fs::Path &fusepath,
                   struct stat *st,
                   fuse_timeouts_t *timeout);
    int process(const Branches &branches,
                const fs::Path &fusepath,
                struct stat *st,
                fuse_timeouts_t *timeout);
  };
}

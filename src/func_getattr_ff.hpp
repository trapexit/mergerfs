#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrFF : public GetAttrBase
  {
  public:
    GetAttrFF() {}
    ~GetAttrFF() {}

  public:
    int process(const Branches &branches,
                const fs::Path &fusepath,
                struct stat *st,
                fuse_timeouts_t *timeout);
  };
}

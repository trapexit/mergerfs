#include "func_getattr_base.hpp"

namespace Func2
{
  class GetattrCombine : public GetattrBase
  {
  public:
    GetattrCombine() {}
    ~GetattrCombine() {}

  public:
    int process(const Branches &branches,
                const fs::Path &fusepath,
                struct stat *st,
                fuse_timeouts_t *timeout);
  };
}

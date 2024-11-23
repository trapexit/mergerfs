#include "func_getattr_base.hpp"

namespace Func2
{
  class GetattrNewest : public GetattrBase
  {
  public:
    GetattrNewest() {}
    ~GetattrNewest() {}

  public:
    int process(const Branches &branches,
                const fs::Path &fusepath,
                struct stat *st,
                fuse_timeouts_t *timeout);
  };
}

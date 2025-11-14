#include "func_statx_base.hpp"

namespace Func2
{
  class StatxCDFO : public StatxBase
  {
  public:
    StatxCDFO() {}
    ~StatxCDFO() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches           &branches,
                   const fs::path           &fusepath,
                   const u32                 flags_,
                   const u32                 mask_,
                   struct fuse_statx        *st,
                   const FollowSymlinksEnum  follow_symlinks,
                   const s64                 symlinkify_timeout);
  };
}

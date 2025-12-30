#include "func_mknod_mfs.hpp"

#include "errno.hpp"
#include "error.hpp"
#include "fs_acl.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_mknod_as.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

std::string_view
Func2::MknodMFS::name() const
{
  return "mfs";
}

int
Func2::MknodMFS::operator()(const ugid_t  &ugid_,
                            const Branches &branches_,
                            const fs::path &fusepath_,
                            const mode_t    mode_,
                            const dev_t     dev_,
                            const mode_t    umask_)
{
  return -EIO;
}

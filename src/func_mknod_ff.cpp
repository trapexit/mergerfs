#include "func_mknod_ff.hpp"

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
Func2::MknodFF::name() const
{
  return "ff";
}

int
Func2::MknodFF::operator()(const ugid_t  &ugid_,
                           const Branches &branches_,
                           const fs::path &fusepath_,
                           const mode_t    mode_,
                           const mode_t    umask_,
                           const dev_t     dev_)
{
  int rv;
  fs::path path;
  fs::info_t info;

  for(const auto &dst_branch : branches_)
    {
      if(dst_branch.ro_or_nc())
        continue;
      rv = fs::info(dst_branch.path,&info);
      if(rv < 0)
        continue;
      if(info.readonly)
        continue;
      if(info.spaceavail < branch.minfreespace())
        continue;

      path = dst_branch.path / fusepath_;

      rv = fs::mknod_as(ugid_,path,mode_,dev_);
      if(rv == 0)
        return 0;
      if(rv != -ENOENT)
        continue;

      path = fusepath_.parent_path();
      for(const auto &src_branch : branches_)
        {
          rv = fs::clonepath(src_branch.path,
                             dst_branch.path,
                             path);
          if(rv == 0)
            break;
        }
    }

  return 0;
}

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
  fs::info_t info;
  fs::path fusedirpath;

  fusedirpath = fusepath_.parent_path();

  const Branch *create_branch = nullptr;
  for(const auto &branch : branches_)
    {
      if(branch.ro_or_nc())
        continue;
      rv = fs::info(branch.path, &info);
      if(rv < 0)
        continue;

      if(info.readonly)
        continue;

      if(info.spaceavail < branch.minfreespace())
        continue;

      create_branch = &branch;
      break;
    }

  if(!create_branch)
    return -ENOSPC;

  // Clone path if needed
  if(existing_branch != create_branch)
    {
      rv = fs::clonepath(existing_branch->path,
                         create_branch->path,
                         fusedirpath);
      if(rv < 0)
        return rv;
    }

  // Create the node
  fs::path fullpath = create_branch->path / fusepath_;
  if(!fs::acl::dir_has_defaults(fullpath))
    {
      mode_t mode = mode_ & ~umask_;
      return fs::mknod_as(ugid_, fullpath, mode, dev_);
    }
  else
    {
      return fs::mknod_as(ugid_, fullpath, mode_, dev_);
    }
}

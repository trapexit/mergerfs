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
                           const mode_t    umask_,
                           const dev_t     dev_)
{
  int rv;
  fs::path fusedirpath;
  mode_t umask;

  fusedirpath = fusepath_.parent_path();
  umask = 0; // TODO: get actual umask

  // Find first branch with the directory
  const Branch *existing_branch = nullptr;
  for(const auto &branch : branches_)
    {
      if(fs::exists(branch.path / fusedirpath))
        {
          existing_branch = &branch;
          break;
        }
    }

  if(!existing_branch)
    return -ENOENT;

  // Find branch with most free space for creation
  const Branch *create_branch = nullptr;
  uint64_t max_space = 0;

  for(const auto &branch : branches_)
    {
      if(branch.ro_or_nc())
        continue;

      fs::info_t info;
      rv = fs::info(branch.path, &info);
      if(rv < 0)
        continue;

      if(info.readonly)
        continue;

      if(info.spaceavail < branch.minfreespace())
        continue;

      if(info.spaceavail > max_space)
        {
          max_space = info.spaceavail;
          create_branch = &branch;
        }
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
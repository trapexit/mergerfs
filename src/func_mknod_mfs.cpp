#include "func_mknod_mfs.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_mknod_as.hpp"


#define error_and_continue(CUR,ERR)             \
  do {                                          \
    ::_calc_error(CUR,ERR);                     \
    continue;                                   \
  } while(0)

static
inline
void
_calc_error(int &cur_,
            const int err_)
{
  switch(cur_)
    {
    default:
    case ENOENT:
      cur_ = err_;
      break;
    case ENOSPC:
      if(err_ != ENOENT)
        cur_ = err_;
      break;
    case EROFS:
      if((err_ != ENOENT) && (err_ != ENOSPC))
        cur_ = err_;
      break;
    }
}


std::string_view
Func2::MknodMFS::name() const
{
  return "mfs";
}

int
Func2::MknodMFS::operator()(const ugid_t   &ugid_,
                            const Branches &branches_,
                            const fs::path &fusepath_,
                            const mode_t    mode_,
                            const dev_t     dev_,
                            const mode_t    umask_)
{
  int rv;
  int error;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  const Branch *clone_src;
  u64 mfs;
  fs::path fullpath;

  branches  = branches_;
  chosen    = nullptr;
  clone_src = nullptr;
  error     = ENOENT;
  mfs = 0;

  for(auto &branch : *branches)
    {
      if(branch.ro_or_nc())
        error_and_continue(error,EROFS);
      rv = fs::info(branch.path,&info);
      if(rv < 0)
        error_and_continue(error,ENOENT);
      if(info.readonly)
        error_and_continue(error,EROFS);
      if(info.spaceavail < branch.minfreespace())
        error_and_continue(error,ENOSPC);

      if(info.spaceavail < mfs)
        continue;

      mfs    = info.spaceavail;
      chosen = &branch;
    }

  if(!chosen)
    return -error;

  for(auto &branch : *branches)
    {
      if(!fs::exists(branch.path,fusepath_.parent_path()))
        continue;
      clone_src = &branch;
      break;
    }

  if(!clone_src)
    return -ENOENT;

  if(clone_src != chosen)
    {
      rv = fs::clonepath(clone_src->path,chosen->path,fusepath_.parent_path());
      if(rv < 0)
        return rv;
    }

  fullpath = chosen->path / fusepath_;

  return fs::mknod_as(ugid_,fullpath,mode_,dev_,umask_);
}

#include "func_symlink_epmfs.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_inode.hpp"
#include "fs_lstat.hpp"
#include "fs_symlink_as.hpp"


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
Func2::SymlinkEPMFS::name() const
{
  return "epmfs";
}

int
Func2::SymlinkEPMFS::operator()(const ugid_t    &ugid_,
                                const Branches  &branches_,
                                const char      *const &target_,
                                const fs::path  &linkpath_,
                                struct stat     *&st_)
{
  int rv;
  int error;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  u64 mfs;
  fs::path fullpath;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  mfs = 0;

  for(auto &branch : *branches)
    {
      if(branch.ro_or_nc())
        error_and_continue(error,EROFS);
      if(!fs::exists(branch.path,linkpath_.parent_path()))
        error_and_continue(error,ENOENT);
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

  fullpath = chosen->path / linkpath_;

  rv = fs::symlink_as(ugid_,target_,fullpath);
  if((rv >= 0) && (st_ != NULL) && (st_->st_ino == 0))
    {
      fs::lstat(fullpath,st_);
      if(st_->st_ino != 0)
        fs::inode::calc(chosen->path,linkpath_,st_);
    }

  return rv;
}

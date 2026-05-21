#include "func_mknod_eplus.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_mknod_as.hpp"
#include <limits>


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
Func2::MknodEPLUS::name() const
{
  return "eplus";
}

int
Func2::MknodEPLUS::operator()(const ugid_t   &ugid_,
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
  u64 lus;
  fs::path fullpath;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  lus = std::numeric_limits<u64>::max();

  for(auto &branch : *branches)
    {
      if(branch.ro_or_nc())
        error_and_continue(error,EROFS);
      if(!fs::exists(branch.path,fusepath_.parent_path()))
        error_and_continue(error,ENOENT);
      rv = fs::info(branch.path,&info);
      if(rv < 0)
        error_and_continue(error,ENOENT);
      if(info.readonly)
        error_and_continue(error,EROFS);
      if(info.spaceavail < branch.minfreespace())
        error_and_continue(error,ENOSPC);

      if(info.spaceused > lus)
        continue;

      lus    = info.spaceused;
      chosen = &branch;
    }

  if(!chosen)
    return -error;

  fullpath = chosen->path / fusepath_;

  return fs::mknod_as(ugid_,fullpath,mode_,dev_,umask_);
}

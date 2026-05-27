#include "func_mknod_all.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_mknod_as.hpp"
#include "fs_path.hpp"


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

#define error_and_continue(CUR,ERR)             \
  do {                                          \
    ::_calc_error(CUR,ERR);                     \
    continue;                                   \
  } while(0)


std::string_view
Func2::MknodAll::name() const
{
  return "all";
}

int
Func2::MknodAll::operator()(const ugid_t   &ugid_,
                            const Branches &branches_,
                            const fs::path &fusepath_,
                            const mode_t    mode_,
                            const dev_t     dev_,
                            const mode_t    umask_)
{
  int rv;
  int error;
  bool any;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *clone_src;
  fs::path fullpath;

  branches  = branches_;
  clone_src = nullptr;
  error     = ENOENT;
  any       = false;

  for(auto &branch : *branches)
    {
      if(!fs::exists(branch.path,fusepath_.parent_path()))
        continue;
      clone_src = &branch;
      break;
    }

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

      if(clone_src && (clone_src != &branch))
        {
          rv = fs::clonepath(clone_src->path,branch.path,fusepath_.parent_path());
          if(rv < 0)
            error_and_continue(error,-rv);
        }

      fullpath = branch.path / fusepath_;
      rv = fs::mknod_as(ugid_,fullpath,mode_,dev_,umask_);
      if(rv < 0)
        {
          ::_calc_error(error,-rv);
          continue;
        }

      any = true;
    }

  if(!any)
    return -error;

  return 0;
}

#include "func_symlink_msplfs.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_inode.hpp"
#include "fs_lstat.hpp"
#include "fs_symlink_as.hpp"
#include "fs_mkdir_as.hpp"
#include <vector>
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
Func2::SymlinkMSPLFS::name() const
{
  return "msplfs";
}

int
Func2::SymlinkMSPLFS::operator()(const ugid_t    &ugid_,
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
  fs::path walk;
  std::vector<fs::path> segments;
  fs::path fullpath;
  mode_t dirmode;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  walk     = linkpath_.parent_path();

  for(;;)
    {
      bool any;
      u64 lfs;

      any = false;
      lfs = std::numeric_limits<u64>::max();
      chosen = nullptr;

      for(auto &branch : *branches)
        {
          if(branch.ro_or_nc())
            error_and_continue(error,EROFS);
          if(!fs::exists(branch.path,walk))
            error_and_continue(error,ENOENT);
          rv = fs::info(branch.path,&info);
          if(rv < 0)
            error_and_continue(error,ENOENT);
          if(info.readonly)
            error_and_continue(error,EROFS);
          if(info.spaceavail < branch.minfreespace())
            error_and_continue(error,ENOSPC);

          any = true;
          if(info.spaceavail > lfs)
            continue;
          lfs    = info.spaceavail;
          chosen = &branch;
        }

      if(any)
        break;
      if(walk == "/")
        break;
      walk = walk.parent_path();
    }

  if(!chosen)
    return -error;

  for(fs::path p = linkpath_.parent_path();
      !p.empty() && p != "/";
      p = p.parent_path())
    segments.push_back(p);

  dirmode = 0755;
  for(auto it = segments.rbegin(); it != segments.rend(); ++it)
    {
      const Branch *src;
      const fs::path full = chosen->path / *it;

      if(fs::exists(full))
        continue;

      src = nullptr;
      for(auto &b : *branches)
        {
          if(&b == chosen)
            continue;
          if(!fs::exists(b.path,*it))
            continue;
          src = &b;
          break;
        }

      if(src != nullptr)
        rv = fs::clonepath(src->path,chosen->path,*it);
      else
        rv = fs::mkdir_as(ugid_,full,dirmode);
      if((rv < 0) && (rv != -EEXIST))
        return rv;
    }

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

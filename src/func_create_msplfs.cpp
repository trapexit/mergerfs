#include "func_create_msplfs.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_open_as.hpp"
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
Func2::CreateMSPLFS::name() const
{
  return "msplfs";
}

bool
Func2::CreateMSPLFS::path_preserving() const
{
  return true;
}


int
Func2::CreateMSPLFS::operator()(const ugid_t      &ugid_,
                                const Branches    &branches_,
                                const fs::path    &fusepath_,
                                fuse_file_info_t *&ffi_,
                                const mode_t      &mode_,
                                const mode_t      &umask_)
{
  int rv;
  int error;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  fs::path walk;
  std::vector<fs::path> segments;
  int fd;
  fs::path fullpath;
  mode_t mode;
  FileInfo *fi;
  mode_t dirmode;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  walk     = fusepath_.parent_path();

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

  for(fs::path p = fusepath_.parent_path();
      !p.empty() && p != "/";
      p = p.parent_path())
    segments.push_back(p);

  dirmode = (0777 & ~umask_);
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

  fullpath = chosen->path / fusepath_;
  mode = mode_;
  if(!fs::acl::dir_has_defaults(fullpath))
    mode &= ~umask_;

  fd = fs::open_as(ugid_,fullpath,ffi_->flags,mode);
  if(fd < 0)
    return fd;

  fi = new FileInfo(fd,chosen,fusepath_,ffi_->direct_io);
  ffi_->fh = fi->to_fh();

  return 0;
}

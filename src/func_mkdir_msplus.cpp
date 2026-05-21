#include "func_mkdir_msplus.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_acl.hpp"
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
Func2::MkdirMSPLUS::name() const
{
  return "msplus";
}

int
Func2::MkdirMSPLUS::operator()(const ugid_t   &ugid_,
                               const Branches &branches_,
                               const fs::path &fusepath_,
                               const mode_t   &mode_,
                               const mode_t   &umask_)
{
  int rv;
  int error;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  fs::path walk;
  std::vector<fs::path> segments;
  fs::path fullpath;
  mode_t mode;
  mode_t dirmode;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  walk     = fusepath_.parent_path();

  for(;;)
    {
      bool any;
      u64 lus;

      any = false;
      lus = std::numeric_limits<u64>::max();
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
          if(info.spaceused > lus)
            continue;
          lus    = info.spaceused;
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

  return fs::mkdir_as(ugid_,fullpath,mode);
}

#include "func_mknod_eppfrd.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_mknod_as.hpp"
#include "rnd.hpp"
#include <vector>


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
Func2::MknodEPPFRD::name() const
{
  return "eppfrd";
}

int
Func2::MknodEPPFRD::operator()(const ugid_t   &ugid_,
                               const Branches &branches_,
                               const fs::path &fusepath_,
                               const mode_t    mode_,
                               const dev_t     dev_,
                               const mode_t    umask_)
{
  int rv;
  int error;
  u64 sum;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  struct W { Branch *b; u64 weight; };
  std::vector<W> writable;
  fs::path fullpath;

  branches = branches_;
  chosen   = nullptr;
  error    = ENOENT;
  sum      = 0;
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

      writable.push_back({&branch,info.spaceavail});
      sum += info.spaceavail;
    }

  if(writable.empty())
    return -error;
  if(sum == 0)
    return -ENOSPC;

  {
    u64 idx;
    u64 threshold;

    idx = 0;
    threshold = RND::rand64(sum);
    for(const auto &w : writable)
      {
        if(w.weight == 0)
          continue;
        idx += w.weight;
        if(idx > threshold)
          {
            chosen = w.b;
            break;
          }
      }
    if(!chosen)
      chosen = writable.back().b;
  }

  fullpath = chosen->path / fusepath_;

  return fs::mknod_as(ugid_,fullpath,mode_,dev_,umask_);
}

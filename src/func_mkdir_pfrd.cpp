#include "func_mkdir_pfrd.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_acl.hpp"
#include "fs_mkdir_as.hpp"
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
Func2::MkdirPFRD::name() const
{
  return "pfrd";
}

int
Func2::MkdirPFRD::operator()(const ugid_t   &ugid_,
                             const Branches &branches_,
                             const fs::path &fusepath_,
                             const mode_t   &mode_,
                             const mode_t   &umask_)
{
  int rv;
  int error;
  u64 sum;
  fs::info_t info;
  Branches::Ptr branches;
  const Branch *chosen;
  const Branch *clone_src;
  struct W { Branch *b; u64 weight; };
  std::vector<W> writable;
  fs::path fullpath;
  mode_t mode;

  branches  = branches_;
  chosen    = nullptr;
  clone_src = nullptr;
  error     = ENOENT;
  sum       = 0;
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
  mode = mode_;
  if(!fs::acl::dir_has_defaults(fullpath))
    mode &= ~umask_;

  return fs::mkdir_as(ugid_,fullpath,mode);
}

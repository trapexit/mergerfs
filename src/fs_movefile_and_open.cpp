/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "fs_movefile_and_open.hpp"

#include "base_types.h"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_copyfile.hpp"
#include "fs_file_size.hpp"
#include "fs_getfl.hpp"
#include "fs_has_space.hpp"
#include "fs_info.hpp"
#include "fs_info_t.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_unlink.hpp"
#include "rnd.hpp"

#include <vector>

#include <fcntl.h>


static
int
_cleanup_flags(const int flags_)
{
  int rv;

  rv = flags_;
  rv = (rv & ~O_TRUNC);
  rv = (rv & ~O_CREAT);
  rv = (rv & ~O_EXCL);

  return rv;
}

// FF pick: first writable branch (excluding the source).
static
int
_pick_ff_branch(const Branches &branches_,
                const fs::path &exclude_path_,
                Branch        *&chosen_)
{
  int err = ENOENT;
  Branches::Ptr branches = branches_;
  fs::info_t info;

  for(auto &branch : *branches)
    {
      if(branch.path == exclude_path_)
        continue;
      if(branch.ro_or_nc())
        { if(err == ENOENT) err = EROFS; continue; }
      const int irv = fs::info(branch.path,&info);
      if(irv < 0)
        { if(err == ENOENT) err = ENOENT; continue; }
      if(info.readonly)
        { if(err == ENOENT) err = EROFS; continue; }
      if(info.spaceavail < branch.minfreespace())
        { if(err == ENOENT) err = ENOSPC; continue; }

      chosen_ = &branch;
      return 0;
    }
  return -err;
}

// MFS pick: most-free-space branch (excluding the source).
static
int
_pick_mfs_branch(const Branches &branches_,
                 const fs::path &exclude_path_,
                 Branch        *&chosen_)
{
  int err = ENOENT;
  Branches::Ptr branches = branches_;
  fs::info_t info;
  uint64_t best = 0;

  for(auto &branch : *branches)
    {
      if(branch.path == exclude_path_)
        continue;
      if(branch.ro_or_nc())
        { if(err == ENOENT) err = EROFS; continue; }
      const int irv = fs::info(branch.path,&info);
      if(irv < 0)
        { if(err == ENOENT) err = ENOENT; continue; }
      if(info.readonly)
        { if(err == ENOENT) err = EROFS; continue; }
      if(info.spaceavail < branch.minfreespace())
        { if(err == ENOENT) err = ENOSPC; continue; }

      if(chosen_ && info.spaceavail <= best)
        continue;
      best = info.spaceavail;
      chosen_ = &branch;
    }
  return chosen_ ? 0 : -err;
}

// LFS pick: least-free-space branch (excluding the source).
static
int
_pick_lfs_branch(const Branches &branches_,
                 const fs::path &exclude_path_,
                 Branch        *&chosen_)
{
  int err = ENOENT;
  Branches::Ptr branches = branches_;
  fs::info_t info;
  uint64_t best = UINT64_MAX;

  for(auto &branch : *branches)
    {
      if(branch.path == exclude_path_)
        continue;
      if(branch.ro_or_nc())
        { if(err == ENOENT) err = EROFS; continue; }
      const int irv = fs::info(branch.path,&info);
      if(irv < 0)
        { if(err == ENOENT) err = ENOENT; continue; }
      if(info.readonly)
        { if(err == ENOENT) err = EROFS; continue; }
      if(info.spaceavail < branch.minfreespace())
        { if(err == ENOENT) err = ENOSPC; continue; }

      if(info.spaceavail >= best)
        continue;
      best = info.spaceavail;
      chosen_ = &branch;
    }
  return chosen_ ? 0 : -err;
}

static
int
_pick_pfrd_branch(const Branches &branches_,
                  const fs::path &exclude_path_,
                  Branch        *&chosen_)
{
  int err = ENOENT;
  Branches::Ptr branches = branches_;
  fs::info_t info;
  struct W { Branch *b; uint64_t weight; };
  std::vector<W> writable;
  uint64_t sum = 0;

  for(auto &branch : *branches)
    {
      // Never reselect the source branch — the caller is here precisely
      // because writing to this branch failed (ENOSPC/quota). Picking it
      // again would cause copyfile+unlink to delete the file.
      if(branch.path == exclude_path_)
        continue;
      if(branch.ro_or_nc())
        { if(err == ENOENT) err = EROFS; continue; }
      const int irv = fs::info(branch.path,&info);
      if(irv < 0)
        { if(err == ENOENT) err = ENOENT; continue; }
      if(info.readonly)
        { if(err == ENOENT) err = EROFS; continue; }
      if(info.spaceavail < branch.minfreespace())
        { if(err == ENOENT) err = ENOSPC; continue; }

      writable.push_back({&branch,info.spaceavail});
      sum += info.spaceavail;
    }

  if(writable.empty())
    return -err;
  if(sum == 0)
    return -ENOSPC;

  const uint64_t threshold = RND::rand64(sum);
  uint64_t idx = 0;
  for(const auto &w : writable)
    {
      if(w.weight == 0) continue;
      idx += w.weight;
      if(idx > threshold) { chosen_ = w.b; return 0; }
    }

  chosen_ = writable.back().b;
  return 0;
}

// Dispatch on policy name. Implements the most common create policies
// (ff, mfs, lfs, pfrd) inline; any other valid create policy name falls
// back to pfrd (the historical default for moveonenospc).
static
int
_pick_branch(const std::string &policy_name_,
             const Branches    &branches_,
             const fs::path    &exclude_path_,
             Branch           *&chosen_)
{
  if(policy_name_ == "ff")
    return ::_pick_ff_branch(branches_,exclude_path_,chosen_);
  if(policy_name_ == "mfs" || policy_name_ == "epmfs" || policy_name_ == "mspmfs")
    return ::_pick_mfs_branch(branches_,exclude_path_,chosen_);
  if(policy_name_ == "lfs" || policy_name_ == "eplfs" || policy_name_ == "msplfs")
    return ::_pick_lfs_branch(branches_,exclude_path_,chosen_);

  return ::_pick_pfrd_branch(branches_,exclude_path_,chosen_);
}

static
int
_movefile_and_open(const std::string &policy_name_,
                   const Branches    &branches_,
                   const fs::path    &branchpath_,
                   const fs::path    &fusepath_,
                   int                origfd_)
{
  int rv;
  int dstfd_flags;
  int origfd_flags;
  s64 src_size;
  fs::path fusedir;
  fs::path src_branch;
  fs::path src_filepath;
  fs::path dst_filepath;
  Branch *dst_branch = nullptr;

  src_branch = branchpath_;

  rv = ::_pick_branch(policy_name_,branches_,branchpath_,dst_branch);
  if(rv < 0)
    return rv;
  if(dst_branch == nullptr)
    return -ENOSPC;

  origfd_flags = fs::getfl(origfd_);
  if(origfd_flags < 0)
    return origfd_flags;

  src_size = fs::file_size(origfd_);
  if(src_size < 0)
    return src_size;

  if(fs::has_space(dst_branch->path,src_size) == false)
    return -ENOSPC;

  fusedir = fusepath_.parent_path();

  rv = fs::clonepath(src_branch,dst_branch->path,fusedir);
  if(rv < 0)
    return -ENOSPC;

  src_filepath = src_branch / fusepath_;
  dst_filepath = dst_branch->path / fusepath_;

  rv = fs::copyfile(src_filepath,dst_filepath,{.cleanup_failure = true});
  if(rv < 0)
    return -ENOSPC;

  dstfd_flags = ::_cleanup_flags(origfd_flags);
  rv = fs::open(dst_filepath,dstfd_flags);
  if(rv < 0)
    return -ENOSPC;

  fs::unlink(src_filepath);

  return rv;
}

int
fs::movefile_and_open(const std::string &policy_name_,
                      const Branches    &branches_,
                      const fs::path    &branchpath_,
                      const fs::path    &fusepath_,
                      const int          origfd_)
{
  return ::_movefile_and_open(policy_name_,branches_,branchpath_,fusepath_,origfd_);
}

int
fs::movefile_and_open_as_root(const std::string &policy_name_,
                              const Branches    &branches_,
                              const fs::path    &branchpath_,
                              const fs::path    &fusepath_,
                              const int          origfd_)
{
  return fs::movefile_and_open(policy_name_,branches_,branchpath_,fusepath_,origfd_);
}

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

// Pick a writable destination branch using PFRD (proportional to free
// space). Self-contained for moveonenospc — mirrors the same algorithm
// used by Func2::CreatePFRD but on a fresh selection (no clonepath/open).
static
int
_pick_pfrd_branch(const Branches &branches_,
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

static
int
_movefile_and_open(const Branches &branches_,
                   const fs::path &branchpath_,
                   const fs::path &fusepath_,
                   int             origfd_)
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

  rv = ::_pick_pfrd_branch(branches_,dst_branch);
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
fs::movefile_and_open(const Branches &branches_,
                      const fs::path &branchpath_,
                      const fs::path &fusepath_,
                      const int       origfd_)
{
  return ::_movefile_and_open(branches_,branchpath_,fusepath_,origfd_);
}

int
fs::movefile_and_open_as_root(const Branches &branches_,
                              const fs::path &branchpath_,
                              const fs::path &fusepath_,
                              const int       origfd_)
{
  return fs::movefile_and_open(branches_,branchpath_,fusepath_,origfd_);
}

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
#include "fs_close.hpp"
#include "fs_copyfile.hpp"
#include "fs_file_size.hpp"
#include "fs_findonfs.hpp"
#include "fs_getfl.hpp"
#include "fs_has_space.hpp"
#include "fs_lstat.hpp"
#include "fs_mktemp.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_rename.hpp"
#include "fs_stat.hpp"
#include "fs_unlink.hpp"
#include "policy.hpp"

#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


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

// A duplicate on any branch could be exposed when the source is unlinked.
// Check even branches that the create policy cannot select.
static
int
_destination_candidates(const fs::path      &src_branch_,
                        const fs::path      &fusepath_,
                        const Branches::Ptr &branches_,
                        Branches::Ptr       *candidates_)
{
  int rv;
  struct stat src_st;

  rv = fs::stat(src_branch_,&src_st);
  if(rv < 0)
    return rv;

  for(const Branch &branch : *branches_)
    {
      struct stat st;

      rv = fs::stat(branch.path.c_str(),&st);
      if(rv < 0)
        return rv;
      if((st.st_dev == src_st.st_dev) &&
         (st.st_ino == src_st.st_ino))
        continue;

      rv = fs::lstat((branch.path / fusepath_).c_str(),&st);
      if(rv == 0)
        return -EEXIST;
      if(rv != -ENOENT)
        return rv;
      if(branch.ro_or_nc())
        continue;

      if(!*candidates_)
        {
          *candidates_ = std::make_shared<Branches::Impl>(&branches_->minfreespace());
          (*candidates_)->reserve(branches_->size());
        }
      (*candidates_)->push_back(branch);
    }

  return 0;
}

static
int
_movefile_and_open(const Policy::Create &createFunc_,
                   const Branches::Ptr  &branches_,
                   const fs::path       &branchpath_,
                   const fs::path       &fusepath_,
                   int                   origfd_)
{
  int rv;
  int dstfd_flags;
  int origfd_flags;
  s64 src_size;
  fs::path fusedir;
  fs::path src_branch;
  fs::path src_filepath;
  fs::path dst_filepath;
  Branches::Ptr candidates;
  std::vector<Branch*> dst_branch;

  // `branchpath_` is where the file was opened. An earlier move on the
  // same handle leaves the fd on another branch.
  src_branch = branchpath_;
  rv = fs::findonfs(branches_,fusepath_,origfd_,&src_branch);
  if(rv < 0)
    return rv;

  rv = ::_destination_candidates(src_branch,fusepath_,branches_,&candidates);
  if(rv < 0)
    return rv;
  if(!candidates)
    return -ENOSPC;

  rv = createFunc_(candidates,fusepath_,dst_branch);
  if(rv < 0)
    return rv;
  if(dst_branch.empty())
    return -ENOSPC;

  origfd_flags = fs::getfl(origfd_);
  if(origfd_flags < 0)
    return origfd_flags;

  src_size = fs::file_size(origfd_);
  if(src_size < 0)
    return src_size;

  if(fs::has_space(dst_branch[0]->path,src_size) == false)
    return -ENOSPC;

  fusedir = fusepath_.parent_path();

  rv = fs::clonepath(src_branch,dst_branch[0]->path,fusedir);
  if(rv < 0)
    return -ENOSPC;

  src_filepath = src_branch / fusepath_;
  dst_filepath = dst_branch[0]->path / fusepath_;

  rv = fs::copyfile(src_filepath,dst_filepath,
                    {.cleanup_failure = true, .no_replace = true});
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
fs::movefile_and_open(const Policy::Create &policy_,
                      const Branches::Ptr  &branches_,
                      const fs::path       &branchpath_,
                      const fs::path       &fusepath_,
                      const int             origfd_)
{
  return ::_movefile_and_open(policy_,
                              branches_,
                              branchpath_,
                              fusepath_,
                              origfd_);
}

int
fs::movefile_and_open_as_root(const Policy::Create &policy_,
                              const Branches::Ptr  &branches_,
                              const fs::path       &branchpath_,
                              const fs::path       &fusepath_,
                              const int             origfd_)
{
  return fs::movefile_and_open(policy_,
                               branches_,
                               branchpath_,
                               fusepath_,
                               origfd_);
}

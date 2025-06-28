/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_close.hpp"
#include "fs_copyfile.hpp"
#include "fs_file_size.hpp"
#include "fs_findonfs.hpp"
#include "fs_getfl.hpp"
#include "fs_has_space.hpp"
#include "fs_mktemp.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_rename.hpp"
#include "fs_stat.hpp"
#include "fs_unlink.hpp"
#include "int_types.h"
#include "policy.hpp"
#include "ugid.hpp"

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

static
int
_movefile_and_open(const Policy::Create &createFunc_,
                   const Branches::Ptr  &branches_,
                   const std::string    &branchpath_,
                   const std::string    &fusepath_,
                   int                   origfd_)
{
  int rv;
  int dstfd_flags;
  int origfd_flags;
  s64 src_size;
  std::string fusedir;
  std::string src_branch;
  std::string src_filepath;
  std::string dst_filepath;
  std::vector<Branch*> dst_branch;

  src_branch = branchpath_;

  rv = createFunc_(branches_,fusepath_.c_str(),dst_branch);
  if(rv == -1)
    return -errno;

  origfd_flags = fs::getfl(origfd_);
  if(origfd_flags == -1)
    return -errno;

  src_size = fs::file_size(origfd_);
  if(src_size == -1)
    return -errno;

  if(fs::has_space(dst_branch[0]->path,src_size) == false)
    return -ENOSPC;

  fusedir = fs::path::dirname(fusepath_);

  rv = fs::clonepath(src_branch,dst_branch[0]->path,fusedir);
  if(rv == -1)
    return -ENOSPC;

  src_filepath = fs::path::make(src_branch,fusepath_);
  dst_filepath = fs::path::make(dst_branch[0]->path,fusepath_);

  rv = fs::copyfile(src_filepath,dst_filepath,FS_COPYFILE_CLEANUP_FAILURE);
  if(rv < 0)
    return -ENOSPC;

  dstfd_flags = ::_cleanup_flags(origfd_flags);
  rv = fs::open(dst_filepath,dstfd_flags);
  if(rv == -1)
    return -ENOSPC;

  fs::unlink(src_filepath);

  return rv;
}

int
fs::movefile_and_open(const Policy::Create &policy_,
                      const Branches::Ptr  &branches_,
                      const std::string    &branchpath_,
                      const std::string    &fusepath_,
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
                              const std::string    &branchpath_,
                              const std::string    &fusepath_,
                              const int             origfd_)
{
  const ugid::Set ugid(0,0);

  return fs::movefile_and_open(policy_,
                               branches_,
                               branchpath_,
                               fusepath_,
                               origfd_);
}

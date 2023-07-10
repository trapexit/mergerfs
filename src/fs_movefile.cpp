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

#include "errno.hpp"
#include "fs_clonefile.hpp"
#include "fs_clonepath.hpp"
#include "fs_close.hpp"
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
#include "policy.hpp"
#include "ugid.hpp"

#include <string>
#include <vector>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using std::string;
using std::vector;


static
int
cleanup_flags(const int flags_)
{
  int rv;

  rv = flags_;
  rv = (rv & ~O_TRUNC);
  rv = (rv & ~O_CREAT);
  rv = (rv & ~O_EXCL);

  return rv;
}


namespace l
{
  static
  int
  movefile(const Policy::Create &createFunc_,
           const Branches::CPtr &branches_,
           const string         &fusepath_,
           int                   origfd_)
  {
    int rv;
    int srcfd;
    int dstfd;
    int dstfd_flags;
    int origfd_flags;
    int64_t srcfd_size;
    string fusedir;
    string srcfd_branch;
    string srcfd_filepath;
    string dstfd_filepath;
    string dstfd_tmp_filepath;
    vector<string> dstfd_branch;

    srcfd = -1;
    dstfd = -1;

    rv = fs::findonfs(branches_,fusepath_,origfd_,&srcfd_branch);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,fusepath_,&dstfd_branch);
    if(rv == -1)
      return -errno;

    origfd_flags = fs::getfl(origfd_);
    if(origfd_flags == -1)
      return -errno;

    srcfd_size = fs::file_size(origfd_);
    if(srcfd_size == -1)
      return -errno;

    if(fs::has_space(dstfd_branch[0],srcfd_size) == false)
      return -ENOSPC;

    fusedir = fs::path::dirname(fusepath_);

    rv = fs::clonepath(srcfd_branch,dstfd_branch[0],fusedir);
    if(rv == -1)
      return -ENOSPC;

    srcfd_filepath = srcfd_branch;
    fs::path::append(srcfd_filepath,fusepath_);
    srcfd = fs::open(srcfd_filepath,O_RDONLY);
    if(srcfd == -1)
      return -ENOSPC;

    dstfd_filepath = dstfd_branch[0];
    fs::path::append(dstfd_filepath,fusepath_);
    std::tie(dstfd,dstfd_tmp_filepath) = fs::mktemp(dstfd_filepath,O_WRONLY);
    if(dstfd < 0)
      {
        fs::close(srcfd);
        return -ENOSPC;
      }

    rv = fs::clonefile(srcfd,dstfd);
    if(rv == -1)
      {
        fs::close(srcfd);
        fs::close(dstfd);
        fs::unlink(dstfd_tmp_filepath);
        return -ENOSPC;
      }

    rv = fs::rename(dstfd_tmp_filepath,dstfd_filepath);
    if(rv == -1)
      {
        fs::close(srcfd);
        fs::close(dstfd);
        fs::unlink(dstfd_tmp_filepath);
        return -ENOSPC;
      }

    fs::close(srcfd);
    fs::close(dstfd);

    dstfd_flags = ::cleanup_flags(origfd_flags);
    rv = fs::open(dstfd_filepath,dstfd_flags);
    if(rv == -1)
      {
        fs::unlink(dstfd_tmp_filepath);
        return -ENOSPC;
      }

    fs::unlink(srcfd_filepath);

    return rv;
  }
}

namespace fs
{
  int
  movefile(const Policy::Create &policy_,
           const Branches::CPtr &basepaths_,
           const string         &fusepath_,
           int                   origfd_)
  {
    return l::movefile(policy_,basepaths_,fusepath_,origfd_);
  }

  int
  movefile_as_root(const Policy::Create &policy_,
                   const Branches::CPtr &basepaths_,
                   const string         &fusepath_,
                   int                   origfd_)
  {
    const ugid::Set ugid(0,0);

    return fs::movefile(policy_,basepaths_,fusepath_,origfd_);
  }
}

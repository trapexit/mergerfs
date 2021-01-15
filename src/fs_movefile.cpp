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


namespace l
{
  static
  int
  movefile(const Policy::Create &createFunc_,
           const Branches::CPtr &branches_,
           const string         &fusepath_,
           int                  *origfd_)
  {
    int rv;
    int fdin;
    int fdout;
    int fdin_flags;
    int64_t fdin_size;
    string fusedir;
    string fdin_path;
    string fdout_temp;
    vector<string> fdout_path;

    fdin = *origfd_;

    fdin_flags = fs::getfl(fdin);
    if(fdin_flags == -1)
      return -1;

    rv = fs::findonfs(branches_,fusepath_,fdin,&fdin_path);
    if(rv == -1)
      return -1;

    rv = createFunc_(branches_,fusepath_,&fdout_path);
    if(rv == -1)
      return -1;

    fdin_size = fs::file_size(fdin);
    if(fdin_size == -1)
      return -1;

    if(fs::has_space(fdout_path[0],fdin_size) == false)
      return (errno=ENOSPC,-1);

    fusedir = fs::path::dirname(fusepath_);

    rv = fs::clonepath(fdin_path,fdout_path[0],fusedir);
    if(rv == -1)
      return -1;

    fs::path::append(fdin_path,fusepath_);
    fdin = fs::open(fdin_path,O_RDONLY);
    if(fdin == -1)
      return -1;

    fs::path::append(fdout_path[0],fusepath_);
    fdout_temp = fdout_path[0];
    fdout = fs::mktemp(&fdout_temp,fdin_flags);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    if(rv == -1)
      goto cleanup;

    rv = fs::rename(fdout_temp,fdout_path[0]);
    if(rv == -1)
      goto cleanup;

    // should we care if it fails?
    fs::unlink(fdin_path);

    std::swap(*origfd_,fdout);
    fs::close(fdin);
    fs::close(fdout);

    return 0;

  cleanup:
    rv = errno;
    if(fdin != -1)
      fs::close(fdin);
    if(fdout != -1)
      fs::close(fdout);
    fs::unlink(fdout_temp);
    errno = rv;
    return -1;
  }
}

namespace fs
{
  int
  movefile(const Policy::Create &policy_,
           const Branches::CPtr &basepaths_,
           const string         &fusepath_,
           int                  *origfd_)
  {
    return l::movefile(policy_,basepaths_,fusepath_,origfd_);
  }

  int
  movefile_as_root(const Policy::Create &policy_,
                   const Branches::CPtr &basepaths_,
                   const string         &fusepath_,
                   int                  *origfd_)
  {
    const ugid::Set ugid(0,0);

    return fs::movefile(policy_,basepaths_,fusepath_,origfd_);
  }
}

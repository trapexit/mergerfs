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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "errno.hpp"
#include "fs.hpp"
#include "fs_base_open.hpp"
#include "fs_base_close.hpp"
#include "fs_base_mkstemp.hpp"
#include "fs_base_rename.hpp"
#include "fs_base_unlink.hpp"
#include "fs_base_stat.hpp"
#include "fs_clonefile.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"

using std::string;
using std::vector;

namespace fs
{
  int
  movefile(const vector<string> &basepaths,
           const string         &fusepath,
           const size_t          additional_size,
           int                  &origfd)
  {
    int rv;
    int fdin;
    int fdout;
    int fdin_flags;
    string fusedir;
    string fdin_path;
    string fdout_path;
    string fdout_temp;
    struct stat fdin_st;

    fdin = origfd;

    rv = fs::fstat(fdin,&fdin_st);
    if(rv == -1)
      return -1;

    fdin_flags = fs::getfl(fdin);
    if(rv == -1)
      return -1;

    rv = fs::findonfs(basepaths,fusepath,fdin,fdin_path);
    if(rv == -1)
      return -1;

    fdin_st.st_size += additional_size;
    rv = fs::mfs(basepaths,fdin_st.st_size,fdout_path);
    if(rv == -1)
      return -1;

    fusedir = fs::path::dirname(&fusepath);

    rv = fs::clonepath(fdin_path,fdout_path,fusedir);
    if(rv == -1)
      return -1;

    fs::path::append(fdin_path,fusepath);
    fdin = fs::open(fdin_path,O_RDONLY);
    if(fdin == -1)
      return -1;

    fs::path::append(fdout_path,fusepath);
    fdout_temp = fdout_path;
    fdout = fs::mkstemp(fdout_temp);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    if(rv == -1)
      goto cleanup;

    rv = fs::setfl(fdout,fdin_flags);
    if(rv == -1)
      goto cleanup;

    rv = fs::rename(fdout_temp,fdout_path);
    if(rv == -1)
      goto cleanup;

    // should we care if it fails?
    fs::unlink(fdin_path);

    std::swap(origfd,fdout);
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

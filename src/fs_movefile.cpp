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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>
#include <vector>

#include "fs.hpp"
#include "fs_path.hpp"
#include "fs_clonepath.hpp"
#include "fs_clonefile.hpp"

using std::string;
using std::vector;

namespace fs
{
  int
  movefile(const vector<string> &basepaths,
           const char           *fusepath,
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
    struct stat fdin_st;

    fdin = origfd;

    rv = fstat(fdin,&fdin_st);
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

    fusedir = fusepath;
    fs::path::dirname(fusedir);
    rv = fs::clonepath(fdin_path,fdout_path,fusedir.c_str());
    if(rv == -1)
      return -1;

    fs::path::append(fdin_path,fusepath);
    fdin = ::open(fdin_path.c_str(),O_RDONLY);
    if(fdin == -1)
      return -1;

    fs::path::append(fdout_path,fusepath);
    fdout = ::open(fdout_path.c_str(),fdin_flags|O_CREAT,fdin_st.st_mode);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    if(rv == -1)
      {
        ::close(fdin);
        ::close(fdout);
        ::unlink(fdout_path.c_str());
        return -1;
      }

    rv = ::unlink(fdin_path.c_str());
    if(rv == -1)
      {
        ::close(fdin);
        ::close(fdout);
        ::unlink(fdout_path.c_str());
        return -1;
      }

    ::close(fdin);
    ::close(origfd);

    origfd = fdout;

    return 0;
  }
}

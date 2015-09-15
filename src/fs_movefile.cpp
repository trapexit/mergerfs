/*
  The MIT License (MIT)

  Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
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

    fusedir = fs::path::dirname(fusepath);
    rv = fs::clonepath(fdin_path,fdout_path,fusedir);
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

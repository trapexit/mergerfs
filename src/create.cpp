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

#include <fuse.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "ugid.hpp"
#include "fileinfo.hpp"
#include "fs.hpp"
#include "assert.hpp"

using std::string;
using std::vector;
using mergerfs::FileInfo;
using mergerfs::Policy;

static
int
_create_controlfile(uint64_t &fh)
{
  fh = (uint64_t)new string;

  return 0;
}

static
int
_create(const Policy::Search::Func  searchFunc,
        const Policy::Create::Func  createPathFunc,
        const vector<string>       &srcmounts,
        const string                fusepath,
        const mode_t                mode,
        const int                   flags,
        uint64_t                   &fh)
{
  int fd;
  string path;
  string dirname;
  fs::Path createpath;
  fs::Path existingpath;

  dirname      = fs::dirname(fusepath);
  existingpath = searchFunc(srcmounts,dirname);
  if(existingpath.base.empty())
    return -ENOENT;

  createpath = createPathFunc(srcmounts,dirname);
  if(createpath.base != existingpath.base)
    fs::clonepath(existingpath.base,createpath.base,dirname);

  path = fs::make_path(createpath.base,fusepath);

  fd = ::open(path.c_str(),flags,mode);
  if(fd == -1)
    return -errno;

  fh = (uint64_t)new FileInfo(fd,flags,path);

  return 0;
}

namespace mergerfs
{
  namespace create
  {
    int
    create(const char            *fusepath,
           mode_t                 mode,
           struct fuse_file_info *fileinfo)
    {
      ugid::SetResetGuard   ugid;
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _create_controlfile(fileinfo->fh);

      return _create(config.policy.search,
                     config.policy.create,
                     config.srcmounts,
                     fusepath,
                     mode,
                     fileinfo->flags,
                     fileinfo->fh);
    }
  }
}

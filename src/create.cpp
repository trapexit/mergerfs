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

#include <fuse.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "fileinfo.hpp"
#include "fs_path.hpp"
#include "fs_clonepath.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using namespace mergerfs;

static
int
_create_core(const string &existingpath,
             const string &createpath,
             const char   *fusepath,
             const char   *fusedirpath,
             const mode_t  mode,
             const int     flags,
             uint64_t     &fh)
{
  int fd;
  string fullpath;

  if(createpath != existingpath)
    {
      const ugid::SetRootGuard ugidGuard;
      fs::clonepath(existingpath,createpath,fusedirpath);
    }

  fs::path::make(&createpath,fusepath,fullpath);

  fd = ::open(fullpath.c_str(),flags,mode);
  if(fd == -1)
    return -errno;

  fh = reinterpret_cast<uint64_t>(new FileInfo(fd));

  return 0;
}

static
int
_create(Policy::Func::Search  searchFunc,
        Policy::Func::Create  createFunc,
        const vector<string> &srcmounts,
        const size_t          minfreespace,
        const char           *fusepath,
        const mode_t          mode,
        const int             flags,
        uint64_t             &fh)
{
  int rv;
  string fullpath;
  string fusedirpath;
  const char *fusedirpathcstr;
  vector<const string*> createpaths;
  vector<const string*> existingpaths;

  fusedirpath = fusepath;
  fs::path::dirname(fusedirpath);
  fusedirpathcstr = fusedirpath.c_str();

  rv = searchFunc(srcmounts,fusedirpathcstr,minfreespace,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc(srcmounts,fusedirpathcstr,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  return _create_core(*existingpaths[0],*createpaths[0],
                      fusepath,fusedirpathcstr,
                      mode,flags,fh);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    create(const char     *fusepath,
           mode_t          mode,
           fuse_file_info *ffi)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _create(config.getattr,
                     config.create,
                     config.srcmounts,
                     config.minfreespace,
                     fusepath,
                     (mode & ~fc->umask),
                     ffi->flags,
                     ffi->fh);
    }
  }
}

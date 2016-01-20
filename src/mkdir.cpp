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

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using namespace mergerfs;

static
int
_mkdir(Policy::Func::Search  searchFunc,
       Policy::Func::Create  createFunc,
       const vector<string> &srcmounts,
       const size_t          minfreespace,
       const string         &fusepath,
       const mode_t          mode)
{
  int rv;
  int error;
  string dirname;
  string existingpath;
  vector<string> createpaths;

  dirname = fs::path::dirname(fusepath);
  rv = searchFunc(srcmounts,dirname,minfreespace,existingpath);
  if(rv == -1)
    return -errno;

  rv = createFunc(srcmounts,dirname,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = createpaths.size(); i != ei; i++)
    {
      string &createpath = createpaths[i];

      if(createpath != existingpath)
        {
          const ugid::SetRootGuard ugidGuard;
          fs::clonepath(existingpath,createpath,dirname);
        }

      fs::path::append(createpath,fusepath);

      rv = ::mkdir(createpath.c_str(),mode);

      error = calc_error(rv,error,errno);
    }

  return -error;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    mkdir(const char *fusepath,
          mode_t      mode)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _mkdir(config.getattr,
                    config.mkdir,
                    config.srcmounts,
                    config.minfreespace,
                    fusepath,
                    (mode & ~fc->umask));
    }
  }
}

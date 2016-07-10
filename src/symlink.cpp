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
#include <sys/types.h>
#include <unistd.h>
#include <string>

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
_symlink_loop_core(const string &existingpath,
                   const string &newbasepath,
                   const char   *oldpath,
                   const char   *newpath,
                   const char   *newdirpath,
                   const int     error)
{
  int rv;
  string fullnewpath;

  if(newbasepath != existingpath)
    {
      const ugid::SetRootGuard ugidGuard;
      fs::clonepath(existingpath,newbasepath,newdirpath);
    }

  fs::path::make(&newbasepath,newpath,fullnewpath);

  rv = ::symlink(oldpath,fullnewpath.c_str());

  return calc_error(rv,error,errno);
}

static
int
_symlink_loop(const string                &existingpath,
              const vector<const string*>  newbasepaths,
              const char                  *oldpath,
              const char                  *newpath,
              const char                  *newdirpath)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = newbasepaths.size(); i != ei; i++)
    {
      error = _symlink_loop_core(existingpath,*newbasepaths[i],
                                 oldpath,newpath,newdirpath,
                                 error);
    }

  return -error;
}

static
int
_symlink(Policy::Func::Search  searchFunc,
         Policy::Func::Create  createFunc,
         const vector<string> &srcmounts,
         const uint64_t        minfreespace,
         const char           *oldpath,
         const char           *newpath)
{
  int rv;
  string newdirpath;
  const char *newdirpathcstr;
  vector<const string*> newbasepaths;
  vector<const string*> existingpaths;

  newdirpath = newpath;
  fs::path::dirname(newdirpath);
  newdirpathcstr = newdirpath.c_str();

  rv = searchFunc(srcmounts,newdirpathcstr,minfreespace,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc(srcmounts,newdirpathcstr,minfreespace,newbasepaths);
  if(rv == -1)
    return -errno;

  return _symlink_loop(*existingpaths[0],newbasepaths,
                       oldpath,newpath,newdirpathcstr);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    symlink(const char *oldpath,
            const char *newpath)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _symlink(config.getattr,
                      config.symlink,
                      config.srcmounts,
                      config.minfreespace,
                      oldpath,
                      newpath);
    }
  }
}

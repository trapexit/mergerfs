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

#include <string>
#include <vector>

#include <errno.h>
#include <unistd.h>

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
_single_link(Policy::Func::Search  searchFunc,
             const vector<string> &srcmounts,
             const size_t          minfreespace,
             const string         &base,
             const string         &oldpath,
             const string         &newpath)
{
  int rv;
  const string fulloldpath = fs::path::make(base,oldpath);
  const string fullnewpath = fs::path::make(base,newpath);

  rv = ::link(fulloldpath.c_str(),fullnewpath.c_str());
  if((rv == -1) && (errno == ENOENT))
    {
      string newpathdir;
      vector<string> foundpath;

      newpathdir = fs::path::dirname(newpath);
      rv = searchFunc(srcmounts,newpathdir,minfreespace,foundpath);
      if(rv == -1)
        return -1;

      {
        const ugid::SetRootGuard ugidGuard;
        fs::clonepath(foundpath[0],base,newpathdir);
      }

      rv = ::link(fulloldpath.c_str(),fullnewpath.c_str());
    }

  return rv;
}

static
int
_link(Policy::Func::Search  searchFunc,
      Policy::Func::Action  actionFunc,
      const vector<string> &srcmounts,
      const size_t          minfreespace,
      const string         &oldpath,
      const string         &newpath)
{
  int rv;
  int error;
  vector<string> oldpaths;

  rv = actionFunc(srcmounts,oldpath,minfreespace,oldpaths);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = oldpaths.size(); i != ei; i++)
    {
      rv = _single_link(searchFunc,srcmounts,minfreespace,oldpaths[i],oldpath,newpath);

      error = calc_error(rv,error,errno);
    }

  return -error;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    link(const char *from,
         const char *to)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _link(config.getattr,
                   config.link,
                   config.srcmounts,
                   config.minfreespace,
                   from,
                   to);
    }
  }
}

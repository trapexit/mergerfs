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
_link_create_path_one(const string &oldbasepath,
                      const string &newbasepath,
                      const string &oldfusepath,
                      const string &newfusepath,
                      const string &newfusedirpath,
                      const int     error)
{
  int rv;
  string oldfullpath;
  string newfullpath;

  if(oldbasepath != newbasepath)
    {
      const ugid::SetRootGuard ugidGuard;
      fs::clonepath(newbasepath,oldbasepath,newfusedirpath);
    }

  fs::path::make(oldbasepath,oldfusepath,oldfullpath);
  fs::path::make(oldbasepath,newfusepath,newfullpath);

  rv = ::link(oldfullpath.c_str(),newfullpath.c_str());

  return calc_error(rv,error,errno);
}

static
int
_link_create_path(Policy::Func::Search  searchFunc,
                  Policy::Func::Action  actionFunc,
                  const vector<string> &srcmounts,
                  const size_t          minfreespace,
                  const string         &oldfusepath,
                  const string         &newfusepath)
{
  int rv;
  int error;
  string newbasepath;
  vector<string> toremove;
  vector<string> oldbasepaths;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  const string newfusedirpath = fs::path::dirname(newfusepath);
  rv = searchFunc(srcmounts,newfusedirpath,minfreespace,newbasepath);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = oldbasepaths.size(); i != ei; i++)
    {
      const string &oldbasepath = oldbasepaths[i];

      error = _link_create_path_one(oldbasepath,newbasepath,
                                    oldfusepath,newfusepath,
                                    newfusedirpath,
                                    error);
    }

  return -error;
}

static
int
_clonepath_if_would_create(Policy::Func::Search  searchFunc,
                           Policy::Func::Create  createFunc,
                           const vector<string> &srcmounts,
                           const size_t          minfreespace,
                           const string         &oldbasepath,
                           const string         &oldfusepath,
                           const string         &newfusepath)
{
  int rv;
  string newbasepath;
  string newfusedirpath;

  newfusedirpath = fs::path::dirname(newfusepath);

  rv = createFunc(srcmounts,newfusedirpath,minfreespace,newbasepath);
  if(rv != -1)
    {
      if(oldbasepath == newbasepath)
        {
          rv = searchFunc(srcmounts,newfusedirpath,minfreespace,newbasepath);
          if(rv != -1)
            {
              const ugid::SetRootGuard ugidGuard;
              fs::clonepath(newbasepath,oldbasepath,newfusedirpath);
            }
        }
      else
        {
          rv = -1;
          errno = EXDEV;
        }
    }

  return rv;
}

static
int
_link_preserve_path_one(Policy::Func::Search  searchFunc,
                        Policy::Func::Create  createFunc,
                        const vector<string> &srcmounts,
                        const size_t          minfreespace,
                        const string         &oldbasepath,
                        const string         &oldfusepath,
                        const string         &newfusepath,
                        const int             error)
{
  int rv;
  string oldfullpath;
  string newfullpath;

  fs::path::make(oldbasepath,oldfusepath,oldfullpath);
  fs::path::make(oldbasepath,newfusepath,newfullpath);

  rv = ::link(oldfullpath.c_str(),newfullpath.c_str());
  if((rv == -1) && (errno == ENOENT))
    {
      rv = _clonepath_if_would_create(searchFunc,createFunc,
                                      srcmounts,minfreespace,
                                      oldbasepath,oldfusepath,newfusepath);
      if(rv != -1)
        rv = ::link(oldfullpath.c_str(),newfullpath.c_str());
    }

  return calc_error(rv,error,errno);
}

static
int
_link_preserve_path(Policy::Func::Search  searchFunc,
                    Policy::Func::Action  actionFunc,
                    Policy::Func::Create  createFunc,
                    const vector<string> &srcmounts,
                    const size_t          minfreespace,
                    const string         &oldfusepath,
                    const string         &newfusepath)
{
  int rv;
  int error;
  vector<string> toremove;
  vector<string> oldbasepaths;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = oldbasepaths.size(); i != ei; i++)
    {
      const string &oldbasepath = oldbasepaths[i];

      error = _link_preserve_path_one(searchFunc,createFunc,
                                      srcmounts,minfreespace,
                                      oldbasepath,
                                      oldfusepath,newfusepath,
                                      error);
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

      if(config.create != Policy::epmfs)
        return _link_create_path(config.link,
                                 config.create,
                                 config.srcmounts,
                                 config.minfreespace,
                                 from,
                                 to);

      return _link_preserve_path(config.getattr,
                                 config.link,
                                 config.create,
                                 config.srcmounts,
                                 config.minfreespace,
                                 from,
                                 to);
    }
  }
}

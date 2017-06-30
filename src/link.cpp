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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_link.hpp"
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
_link_create_path_core(const string &oldbasepath,
                       const string &newbasepath,
                       const char   *oldfusepath,
                       const char   *newfusepath,
                       const string &newfusedirpath,
                       const int     error)
{
  int rv;
  string oldfullpath;
  string newfullpath;

  if(oldbasepath != newbasepath)
    {
      const ugid::SetRootGuard ugidGuard;
      rv = fs::clonepath(newbasepath,oldbasepath,newfusedirpath);
      if(rv == -1)
        return errno;
    }

  fs::path::make(&oldbasepath,oldfusepath,oldfullpath);
  fs::path::make(&oldbasepath,newfusepath,newfullpath);

  rv = fs::link(oldfullpath,newfullpath);

  return error::calc(rv,error,errno);
}

static
int
_link_create_path_loop(const vector<const string*> &oldbasepaths,
                       const string                &newbasepath,
                       const char                  *oldfusepath,
                       const char                  *newfusepath,
                       const string                &newfusedirpath)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = oldbasepaths.size(); i != ei; i++)
    {
      error = _link_create_path_core(*oldbasepaths[i],newbasepath,
                                     oldfusepath,newfusepath,
                                     newfusedirpath,
                                     error);
    }

  return -error;
}

static
int
_link_create_path(Policy::Func::Search  searchFunc,
                  Policy::Func::Action  actionFunc,
                  const vector<string> &srcmounts,
                  const uint64_t        minfreespace,
                  const char           *oldfusepath,
                  const char           *newfusepath)
{
  int rv;
  string newfusedirpath;
  vector<const string*> oldbasepaths;
  vector<const string*> newbasepaths;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  newfusedirpath = newfusepath;
  fs::path::dirname(newfusedirpath);
  rv = searchFunc(srcmounts,newfusedirpath.c_str(),minfreespace,newbasepaths);
  if(rv == -1)
    return -errno;

  return _link_create_path_loop(oldbasepaths,*newbasepaths[0],
                                oldfusepath,newfusepath,
                                newfusedirpath);
}

static
int
_clonepath_if_would_create(Policy::Func::Search  searchFunc,
                           Policy::Func::Create  createFunc,
                           const vector<string> &srcmounts,
                           const uint64_t        minfreespace,
                           const string         &oldbasepath,
                           const char           *oldfusepath,
                           const char           *newfusepath)
{
  int rv;
  string newfusedirpath;
  const char *newfusedirpathcstr;
  vector<const string*> newbasepath;

  newfusedirpath = newfusepath;
  fs::path::dirname(newfusedirpath);
  newfusedirpathcstr = newfusedirpath.c_str();

  rv = createFunc(srcmounts,newfusedirpathcstr,minfreespace,newbasepath);
  if(rv == -1)
    return -1;

  if(oldbasepath != *newbasepath[0])
    return (errno=EXDEV,-1);

  rv = searchFunc(srcmounts,newfusedirpathcstr,minfreespace,newbasepath);
  if(rv == -1)
    return -1;

  const ugid::SetRootGuard ugidGuard;
  return fs::clonepath(*newbasepath[0],oldbasepath,newfusedirpathcstr);
}

static
int
_link_preserve_path_core(Policy::Func::Search  searchFunc,
                         Policy::Func::Create  createFunc,
                         const vector<string> &srcmounts,
                         const uint64_t        minfreespace,
                         const string         &oldbasepath,
                         const char           *oldfusepath,
                         const char           *newfusepath,
                         const int             error)
{
  int rv;
  string oldfullpath;
  string newfullpath;

  fs::path::make(&oldbasepath,oldfusepath,oldfullpath);
  fs::path::make(&oldbasepath,newfusepath,newfullpath);

  rv = fs::link(oldfullpath,newfullpath);
  if((rv == -1) && (errno == ENOENT))
    {
      rv = _clonepath_if_would_create(searchFunc,createFunc,
                                      srcmounts,minfreespace,
                                      oldbasepath,
                                      oldfusepath,newfusepath);
      if(rv != -1)
        rv = fs::link(oldfullpath,newfullpath);
    }

  return error::calc(rv,error,errno);
}

static
int
_link_preserve_path_loop(Policy::Func::Search         searchFunc,
                         Policy::Func::Create         createFunc,
                         const vector<string>        &srcmounts,
                         const uint64_t               minfreespace,
                         const char                  *oldfusepath,
                         const char                  *newfusepath,
                         const vector<const string*> &oldbasepaths)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = oldbasepaths.size(); i != ei; i++)
    {
      error = _link_preserve_path_core(searchFunc,createFunc,
                                       srcmounts,minfreespace,
                                       *oldbasepaths[i],
                                       oldfusepath,newfusepath,
                                       error);
    }

  return -error;
}

static
int
_link_preserve_path(Policy::Func::Search  searchFunc,
                    Policy::Func::Action  actionFunc,
                    Policy::Func::Create  createFunc,
                    const vector<string> &srcmounts,
                    const uint64_t        minfreespace,
                    const char           *oldfusepath,
                    const char           *newfusepath)
{
  int rv;
  vector<const string*> oldbasepaths;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  return _link_preserve_path_loop(searchFunc,createFunc,
                                  srcmounts,minfreespace,
                                  oldfusepath,newfusepath,
                                  oldbasepaths);
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

      if(config.create->path_preserving() && !config.ignorepponrename)
        return _link_preserve_path(config.getattr,
                                   config.link,
                                   config.create,
                                   config.srcmounts,
                                   config.minfreespace,
                                   from,
                                   to);

      return _link_create_path(config.link,
                               config.create,
                               config.srcmounts,
                               config.minfreespace,
                               from,
                               to);
    }
  }
}

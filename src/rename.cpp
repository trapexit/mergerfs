/*
  The MIT License (MIT)

  Copyright (c) 2016 Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using std::set;
using mergerfs::Policy;
using namespace mergerfs;

static
bool
member(const vector<string> &haystack,
       const string         &needle)
{
  return (std::find(haystack.begin(),haystack.end(),needle) != haystack.end());
}

// a single success trumps any failure
static
int
_process_rv(const int rv,
            const int preverror,
            const int error)
{
  if(rv == -1)
    {
      if(preverror == 0)
        return 0;
      return error;
    }

  return 0;
}

static
void
_remove(const vector<string> &toremove)
{
  for(size_t i = 0, ei = toremove.size(); i != ei; i++)
    ::remove(toremove[i].c_str());
}

static
void
_rename_create_path_one(const vector<string> &oldbasepaths,
                        const string         &oldbasepath,
                        const string         &newbasepath,
                        const string         &oldfusepath,
                        const string         &newfusepath,
                        const string         &newfusedirpath,
                        int                  &error,
                        vector<string>       &tounlink)
{
  int rv;
  bool ismember;
  string oldfullpath;
  string newfullpath;

  fs::path::make(oldbasepath,newfusepath,newfullpath);

  ismember = member(oldbasepaths,oldbasepath);
  if(ismember)
    {
      if(oldbasepath != newbasepath)
        {
          const ugid::SetRootGuard ugidGuard;
          fs::clonepath(newbasepath,oldbasepath,newfusedirpath);
        }

      fs::path::make(oldbasepath,oldfusepath,oldfullpath);

      rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
      error = _process_rv(rv,error,errno);
      if(rv == -1)
        tounlink.push_back(oldfullpath);
    }
  else
    {
      tounlink.push_back(newfullpath);
    }
}

static
int
_rename_create_path(Policy::Func::Search  searchFunc,
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
  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      const string &oldbasepath = srcmounts[i];

      _rename_create_path_one(oldbasepaths,oldbasepath,newbasepath,
                              oldfusepath,newfusepath,
                              newfusedirpath,
                              error,toremove);
    }

  if(error == 0)
    _remove(toremove);

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
void
_rename_preserve_path_one(Policy::Func::Search  searchFunc,
                          Policy::Func::Create  createFunc,
                          const vector<string> &srcmounts,
                          const size_t          minfreespace,
                          const vector<string> &oldbasepaths,
                          const string         &oldbasepath,
                          const string         &oldfusepath,
                          const string         &newfusepath,
                          int                  &error,
                          vector<string>       &toremove)
{
  int rv;
  bool ismember;
  string newfullpath;

  fs::path::make(oldbasepath,newfusepath,newfullpath);

  ismember = member(oldbasepaths,oldbasepath);
  if(ismember)
    {
      string oldfullpath;

      fs::path::make(oldbasepath,oldfusepath,oldfullpath);

      rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
      if((rv == -1) && (errno == ENOENT))
        {
          rv = _clonepath_if_would_create(searchFunc,createFunc,
                                          srcmounts,minfreespace,
                                          oldbasepath,oldfusepath,newfusepath);
          if(rv != -1)
            rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
        }

      error = _process_rv(rv,error,errno);
      if(rv == -1)
        toremove.push_back(oldfullpath);
    }
  else
    {
      toremove.push_back(newfullpath);
    }
}

static
int
_rename_preserve_path(Policy::Func::Search  searchFunc,
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
  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      const string &oldbasepath = srcmounts[i];

      _rename_preserve_path_one(searchFunc,createFunc,
                                srcmounts,minfreespace,
                                oldbasepaths,oldbasepath,
                                oldfusepath,newfusepath,
                                error,toremove);
    }

  if(error == 0)
    _remove(toremove);

  return -error;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    rename(const char *oldpath,
           const char *newpath)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      if(config.create != Policy::epmfs)
        return _rename_create_path(config.rename,
                                   config.create,
                                   config.srcmounts,
                                   config.minfreespace,
                                   oldpath,
                                   newpath);

      return _rename_preserve_path(config.getattr,
                                   config.rename,
                                   config.create,
                                   config.srcmounts,
                                   config.minfreespace,
                                   oldpath,
                                   newpath);
    }
  }
}

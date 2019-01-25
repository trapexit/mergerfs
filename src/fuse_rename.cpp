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

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_remove.hpp"
#include "fs_base_rename.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using std::set;

static
bool
member(const vector<const string*> &haystack,
       const string                &needle)
{
  for(size_t i = 0, ei = haystack.size(); i != ei; i++)
    {
      if(*haystack[i] == needle)
        return true;
    }

  return false;
}

static
void
_remove(const vector<string> &toremove)
{
  for(size_t i = 0, ei = toremove.size(); i != ei; i++)
    fs::remove(toremove[i]);
}

static
void
_rename_create_path_core(const vector<const string*> &oldbasepaths,
                         const string                &oldbasepath,
                         const string                &newbasepath,
                         const char                  *oldfusepath,
                         const char                  *newfusepath,
                         const string                &newfusedirpath,
                         int                         &error,
                         vector<string>              &tounlink)
{
  int rv;
  bool ismember;
  string oldfullpath;
  string newfullpath;

  ismember = member(oldbasepaths,oldbasepath);
  if(ismember)
    {
      rv = fs::clonepath_as_root(newbasepath,oldbasepath,newfusedirpath);
      if(rv != -1)
        {
          oldfullpath = fs::path::make(oldbasepath,oldfusepath);
          newfullpath = fs::path::make(oldbasepath,newfusepath);

          rv = fs::rename(oldfullpath,newfullpath);
        }

      error = error::calc(rv,error,errno);
      if(rv == -1)
        tounlink.push_back(oldfullpath);
    }
  else
    {
      newfullpath = fs::path::make(oldbasepath,newfusepath);

      tounlink.push_back(newfullpath);
    }
}

static
int
_rename_create_path(Policy::Func::Search  searchFunc,
                    Policy::Func::Action  actionFunc,
                    const Branches       &branches_,
                    const uint64_t        minfreespace,
                    const char           *oldfusepath,
                    const char           *newfusepath)
{
  int rv;
  int error;
  string newfusedirpath;
  vector<string> toremove;
  vector<const string*> newbasepath;
  vector<const string*> oldbasepaths;

  rv = actionFunc(branches_,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  newfusedirpath = fs::path::dirname(newfusepath);

  rv = searchFunc(branches_,newfusedirpath,minfreespace,newbasepath);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = branches_.size(); i != ei; i++)
    {
      const string &oldbasepath = branches_[i].path;

      _rename_create_path_core(oldbasepaths,
                               oldbasepath,*newbasepath[0],
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
_clonepath(Policy::Func::Search  searchFunc,
           const Branches       &branches_,
           const uint64_t        minfreespace,
           const string         &dstbasepath,
           const string         &fusedirpath)
{
  int rv;
  vector<const string*> srcbasepath;

  rv = searchFunc(branches_,fusedirpath,minfreespace,srcbasepath);
  if(rv == -1)
    return -errno;

  fs::clonepath_as_root(*srcbasepath[0],dstbasepath,fusedirpath);

  return 0;
}

static
int
_clonepath_if_would_create(Policy::Func::Search  searchFunc,
                           Policy::Func::Create  createFunc,
                           const Branches       &branches_,
                           const uint64_t        minfreespace,
                           const string         &oldbasepath,
                           const char           *oldfusepath,
                           const char           *newfusepath)
{
  int rv;
  string newfusedirpath;
  vector<const string*> newbasepath;

  newfusedirpath = fs::path::dirname(newfusepath);

  rv = createFunc(branches_,newfusedirpath,minfreespace,newbasepath);
  if(rv == -1)
    return rv;

  if(oldbasepath == *newbasepath[0])
    return _clonepath(searchFunc,branches_,minfreespace,oldbasepath,newfusedirpath);

  return (errno=EXDEV,-1);
}

static
void
_rename_preserve_path_core(Policy::Func::Search         searchFunc,
                           Policy::Func::Create         createFunc,
                           const Branches              &branches_,
                           const uint64_t               minfreespace,
                           const vector<const string*> &oldbasepaths,
                           const string                &oldbasepath,
                           const char                  *oldfusepath,
                           const char                  *newfusepath,
                           int                         &error,
                           vector<string>              &toremove)
{
  int rv;
  bool ismember;
  string newfullpath;

  newfullpath = fs::path::make(oldbasepath,newfusepath);

  ismember = member(oldbasepaths,oldbasepath);
  if(ismember)
    {
      string oldfullpath;

      oldfullpath = fs::path::make(oldbasepath,oldfusepath);

      rv = fs::rename(oldfullpath,newfullpath);
      if((rv == -1) && (errno == ENOENT))
        {
          rv = _clonepath_if_would_create(searchFunc,createFunc,
                                          branches_,minfreespace,
                                          oldbasepath,oldfusepath,newfusepath);
          if(rv == 0)
            rv = fs::rename(oldfullpath,newfullpath);
        }

      error = error::calc(rv,error,errno);
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
                      const Branches       &branches_,
                      const uint64_t        minfreespace,
                      const char           *oldfusepath,
                      const char           *newfusepath)
{
  int rv;
  int error;
  vector<string> toremove;
  vector<const string*> oldbasepaths;

  rv = actionFunc(branches_,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  error = -1;
  for(size_t i = 0, ei = branches_.size(); i != ei; i++)
    {
      const string &oldbasepath = branches_[i].path;

      _rename_preserve_path_core(searchFunc,createFunc,
                                 branches_,minfreespace,
                                 oldbasepaths,oldbasepath,
                                 oldfusepath,newfusepath,
                                 error,toremove);
    }

  if(error == 0)
    _remove(toremove);

  return -error;
}

namespace FUSE
{
  int
  rename(const char *oldpath,
         const char *newpath)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    config.open_cache.erase(oldpath);

    if(config.create->path_preserving() && !config.ignorepponrename)
      return _rename_preserve_path(config.getattr,
                                   config.rename,
                                   config.create,
                                   config.branches,
                                   config.minfreespace,
                                   oldpath,
                                   newpath);

    return _rename_create_path(config.getattr,
                               config.rename,
                               config.branches,
                               config.minfreespace,
                               oldpath,
                               newpath);
  }
}

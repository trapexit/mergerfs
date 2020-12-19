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

#include "config.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"
#include "ugid.hpp"

#include <algorithm>
#include <string>

using std::string;


namespace error
{
  static
  inline
  int
  calc(const int rv_,
       const int prev_,
       const int cur_)
  {
    if(rv_ == -1)
      {
        if(prev_ == 0)
          return 0;
        return cur_;
      }

    return 0;
  }
}

static
bool
member(const StrVec &haystack,
       const string &needle)
{
  for(size_t i = 0, ei = haystack.size(); i != ei; i++)
    {
      if(haystack[i] == needle)
        return true;
    }

  return false;
}

static
void
_remove(const StrVec &toremove)
{
  for(size_t i = 0, ei = toremove.size(); i != ei; i++)
    fs::remove(toremove[i]);
}

static
void
_rename_create_path_core(const StrVec &oldbasepaths,
                         const string &oldbasepath,
                         const string &newbasepath,
                         const char   *oldfusepath,
                         const char   *newfusepath,
                         const string &newfusedirpath,
                         int          &error,
                         StrVec       &tounlink)
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
_rename_create_path(const Policy::Search &searchFunc,
                    const Policy::Action &actionFunc,
                    const Branches::CPtr &branches_,
                    const char           *oldfusepath,
                    const char           *newfusepath)
{
  int rv;
  int error;
  string newfusedirpath;
  StrVec toremove;
  StrVec newbasepath;
  StrVec oldbasepaths;
  StrVec branches;

  rv = actionFunc(branches_,oldfusepath,&oldbasepaths);
  if(rv == -1)
    return -errno;

  newfusedirpath = fs::path::dirname(newfusepath);

  rv = searchFunc(branches_,newfusedirpath.c_str(),&newbasepath);
  if(rv == -1)
    return -errno;

  branches_->to_paths(branches);

  error = -1;
  for(size_t i = 0, ei = branches.size(); i != ei; i++)
    {
      const string &oldbasepath = branches[i];

      _rename_create_path_core(oldbasepaths,
                               oldbasepath,newbasepath[0],
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
_clonepath(const Policy::Search &searchFunc,
           const Branches::CPtr &branches_,
           const string         &dstbasepath,
           const string         &fusedirpath)
{
  int rv;
  StrVec srcbasepath;

  rv = searchFunc(branches_,fusedirpath.c_str(),&srcbasepath);
  if(rv == -1)
    return -errno;

  fs::clonepath_as_root(srcbasepath[0],dstbasepath,fusedirpath);

  return 0;
}

static
int
_clonepath_if_would_create(const Policy::Search &searchFunc,
                           const Policy::Create &createFunc,
                           const Branches::CPtr &branches_,
                           const string         &oldbasepath,
                           const char           *oldfusepath,
                           const char           *newfusepath)
{
  int rv;
  string newfusedirpath;
  StrVec newbasepath;

  newfusedirpath = fs::path::dirname(newfusepath);

  rv = createFunc(branches_,newfusedirpath.c_str(),&newbasepath);
  if(rv == -1)
    return rv;

  if(oldbasepath == newbasepath[0])
    return _clonepath(searchFunc,branches_,oldbasepath,newfusedirpath);

  return (errno=EXDEV,-1);
}

static
void
_rename_preserve_path_core(const Policy::Search &searchFunc,
                           const Policy::Create &createFunc,
                           const Branches::CPtr &branches_,
                           const StrVec         &oldbasepaths,
                           const string         &oldbasepath,
                           const char           *oldfusepath,
                           const char           *newfusepath,
                           int                  &error,
                           StrVec               &toremove)
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
                                          branches_,oldbasepath,
                                          oldfusepath,newfusepath);
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
_rename_preserve_path(const Policy::Search &searchFunc,
                      const Policy::Action &actionFunc,
                      const Policy::Create &createFunc,
                      const Branches::CPtr &branches_,
                      const char           *oldfusepath,
                      const char           *newfusepath)
{
  int rv;
  int error;
  StrVec toremove;
  StrVec oldbasepaths;
  StrVec branches;

  rv = actionFunc(branches_,oldfusepath,&oldbasepaths);
  if(rv == -1)
    return -errno;

  branches_->to_paths(branches);

  error = -1;
  for(size_t i = 0, ei = branches.size(); i != ei; i++)
    {
      const string &oldbasepath = branches[i];

      _rename_preserve_path_core(searchFunc,createFunc,
                                 branches_,
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
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    if(cfg->func.create.policy.path_preserving() && !cfg->ignorepponrename)
      return _rename_preserve_path(cfg->func.getattr.policy,
                                   cfg->func.rename.policy,
                                   cfg->func.create.policy,
                                   cfg->branches,
                                   oldpath,
                                   newpath);

    return _rename_create_path(cfg->func.getattr.policy,
                               cfg->func.rename.policy,
                               cfg->branches,
                               oldpath,
                               newpath);
  }
}

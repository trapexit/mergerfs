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
#include "fs_base_chown.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Config;

static
int
_chown_loop_core(const string *basepath,
                 const char   *fusepath,
                 const uid_t   uid,
                 const gid_t   gid,
                 const int     error)
{
  int rv;
  string fullpath;

  fs::path::make(basepath,fusepath,fullpath);

  rv = fs::lchown(fullpath,uid,gid);

  return error::calc(rv,error,errno);
}

static
int
_chown_loop(const vector<const string*> &basepaths,
            const char                  *fusepath,
            const uid_t                  uid,
            const gid_t                  gid)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      error = _chown_loop_core(basepaths[i],fusepath,uid,gid,error);
    }

  return -error;
}

static
int
_chown(Policy::Func::Action  actionFunc,
       const Branches       &branches_,
       const uint64_t        minfreespace,
       const char           *fusepath,
       const uid_t           uid,
       const gid_t           gid)
{
  int rv;
  vector<const string*> basepaths;

  rv = actionFunc(branches_,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  return _chown_loop(basepaths,fusepath,uid,gid);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    chown(const char *fusepath,
          uid_t       uid,
          gid_t       gid)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.branches_lock);

      return _chown(config.chown,
                    config.branches,
                    config.minfreespace,
                    fusepath,
                    uid,
                    gid);
    }
  }
}

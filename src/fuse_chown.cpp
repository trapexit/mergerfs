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
#include "fs_base_chown.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace l
{
  static
  int
  chown_loop_core(const string *basepath_,
                  const char   *fusepath_,
                  const uid_t   uid_,
                  const gid_t   gid_,
                  const int     error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    rv = fs::lchown(fullpath,uid_,gid_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  chown_loop(const vector<const string*> &basepaths_,
             const char                  *fusepath_,
             const uid_t                  uid_,
             const gid_t                  gid_)
  {
    int error;

    error = -1;
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        error = l::chown_loop_core(basepaths_[i],fusepath_,uid_,gid_,error);
      }

    return -error;
  }

  static
  int
  chown(Policy::Func::Action  actionFunc_,
        const Branches       &branches_,
        const uint64_t        minfreespace_,
        const char           *fusepath_,
        const uid_t           uid_,
        const gid_t           gid_)
  {
    int rv;
    vector<const string*> basepaths;

    rv = actionFunc_(branches_,fusepath_,minfreespace_,basepaths);
    if(rv == -1)
      return -errno;

    return l::chown_loop(basepaths,fusepath_,uid_,gid_);
  }
}

namespace FUSE
{
  int
  chown(const char *fusepath_,
        uid_t       uid_,
        gid_t       gid_)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::chown(config.chown,
                    config.branches,
                    config.minfreespace,
                    fusepath_,
                    uid_,
                    gid_);
  }
}

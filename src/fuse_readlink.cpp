/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_readlink.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_readlink.hpp"
#include "symlinkify.hpp"

#include "fuse.h"

#include <algorithm>
#include <cstring>


static
int
_readlink_core_standard(const fs::path &fullpath_,
                        char           *buf_,
                        const size_t    bufsize_)

{
  int rv;

  rv = fs::readlink(fullpath_,buf_,(bufsize_ - 1));
  if(rv < 0)
    return rv;

  buf_[rv] = '\0';

  return 0;
}

static
int
_readlink_core_symlinkify(const fs::path &fullpath_,
                          char           *buf_,
                          const size_t    bufsize_,
                          const time_t    symlinkify_timeout_)
{
  int rv;
  size_t n;
  std::string fullpath_str;
  struct stat st;

  fullpath_str = fullpath_.string();

  rv = fs::lstat(fullpath_str,&st);
  if(rv < 0)
    return rv;

  if(!symlinkify::can_be_symlink(st,symlinkify_timeout_))
    return ::_readlink_core_standard(fullpath_,buf_,bufsize_);

  n = std::min(fullpath_str.size(),(bufsize_ - 1));
  memcpy(buf_,fullpath_str.c_str(),n);
  buf_[n] = '\0';

  return 0;
}

static
int
_readlink_core(const fs::path &basepath_,
               const fs::path &fusepath_,
               char           *buf_,
               const size_t    bufsize_,
               const bool      symlinkify_,
               const time_t    symlinkify_timeout_)
{
  fs::path fullpath;

  fullpath = basepath_ / fusepath_;

  if(symlinkify_)
    return ::_readlink_core_symlinkify(fullpath,buf_,bufsize_,symlinkify_timeout_);

  return ::_readlink_core_standard(fullpath,buf_,bufsize_);
}

static
int
_readlink(const Policy::Search &searchFunc_,
          const Branches::Ptr   ibranches_,
          const fs::path       &fusepath_,
          char                 *buf_,
          const size_t          bufsize_,
          const bool            symlinkify_,
          const time_t          symlinkify_timeout_)
{
  int rv;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv < 0)
    return rv;
  if(obranches.empty())
    return -ENOENT;

  return ::_readlink_core(obranches[0]->path,
                          fusepath_,
                          buf_,
                          bufsize_,
                          symlinkify_,
                          symlinkify_timeout_);
}

int
FUSE::readlink(const fuse_req_ctx_t *ctx_,
               const char           *fusepath_,
               char                 *buf_,
               size_t                bufsize_)
{
  const fs::path fusepath{fusepath_};

  if(bufsize_ == 0)
    return -EINVAL;

  return ::_readlink(cfg.func.readlink.policy,
                     cfg.branches,
                     fusepath,
                     buf_,
                     bufsize_,
                     cfg.symlinkify,
                     cfg.symlinkify_timeout);
}

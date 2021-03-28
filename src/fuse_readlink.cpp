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
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_readlink.hpp"
#include "symlinkify.hpp"
#include "ugid.hpp"
#include "state.hpp"

#include "fuse.h"

#include <string.h>

using std::string;


namespace l
{
  static
  int
  readlink_core_standard(const string &fullpath_,
                         char         *buf_,
                         const size_t  size_)

  {
    int rv;

    rv = fs::readlink(fullpath_,buf_,size_);
    if(rv == -1)
      return -errno;

    buf_[rv] = '\0';

    return 0;
  }

  static
  int
  readlink_core_symlinkify(const string &fullpath_,
                           char         *buf_,
                           const size_t  size_,
                           const time_t  symlinkify_timeout_)
  {
    int rv;
    struct stat st;

    rv = fs::lstat(fullpath_,&st);
    if(rv == -1)
      return -errno;

    if(!symlinkify::can_be_symlink(st,symlinkify_timeout_))
      return l::readlink_core_standard(fullpath_,buf_,size_);

    strncpy(buf_,fullpath_.c_str(),size_);

    return 0;
  }

  static
  int
  readlink_core(const string &basepath_,
                const char   *fusepath_,
                char         *buf_,
                const size_t  size_,
                const bool    symlinkify_,
                const time_t  symlinkify_timeout_)
  {
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    if(symlinkify_)
      return l::readlink_core_symlinkify(fullpath,buf_,size_,symlinkify_timeout_);

    return l::readlink_core_standard(fullpath,buf_,size_);
  }
}

namespace FUSE::READLINK
{
  int
  readlink(const char *fusepath_,
           char       *buf_,
           size_t      bufsiz_)
  {
    State s;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return s->readlink(fusepath_,buf_,bufsiz_);
  }
}

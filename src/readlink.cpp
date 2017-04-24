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

#include <string.h>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_readlink.hpp"
#include "fs_base_stat.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "symlinkify.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_readlink_core_standard(const string &fullpath,
                        char         *buf,
                        const size_t  size)

{
  int rv;

  rv = fs::readlink(fullpath,buf,size);
  if(rv == -1)
    return -errno;

  buf[rv] = '\0';

  return 0;
}

static
int
_readlink_core_symlinkify(const string &fullpath,
                          char         *buf,
                          const size_t  size,
                          const time_t  symlinkify_timeout)
{
  int rv;
  struct stat st;

  rv = fs::stat(fullpath,st);
  if(rv == -1)
    return -errno;

  if(!symlinkify::can_be_symlink(st,symlinkify_timeout))
    return _readlink_core_standard(fullpath,buf,size);

  strncpy(buf,fullpath.c_str(),size);

  return 0;
}

static
int
_readlink_core(const string *basepath,
               const char   *fusepath,
               char         *buf,
               const size_t  size,
               const bool    symlinkify,
               const time_t  symlinkify_timeout)
{
  string fullpath;

  fs::path::make(basepath,fusepath,fullpath);

  if(symlinkify)
    return _readlink_core_symlinkify(fullpath,buf,size,symlinkify_timeout);

  return _readlink_core_standard(fullpath,buf,size);
}

static
int
_readlink(Policy::Func::Search  searchFunc,
          const vector<string> &srcmounts,
          const uint64_t        minfreespace,
          const char           *fusepath,
          char                 *buf,
          const size_t          size,
          const bool            symlinkify,
          const time_t          symlinkify_timeout)
{
  int rv;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  return _readlink_core(basepaths[0],fusepath,buf,size,
                        symlinkify,symlinkify_timeout);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    readlink(const char *fusepath,
             char       *buf,
             size_t      size)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _readlink(config.readlink,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       buf,
                       size,
                       config.symlinkify,
                       config.symlinkify_timeout);
    }
  }
}

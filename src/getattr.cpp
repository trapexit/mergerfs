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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.hpp"
#include "errno.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_getattr_controlfile(struct stat &buf)
{
  static const uid_t  uid = ::getuid();
  static const gid_t  gid = ::getgid();
  static const time_t now = ::time(NULL);

  buf.st_dev     = 0;
  buf.st_ino     = 0;
  buf.st_mode    = (S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  buf.st_nlink   = 1;
  buf.st_uid     = uid;
  buf.st_gid     = gid;
  buf.st_rdev    = 0;
  buf.st_size    = 0;
  buf.st_blksize = 1024;
  buf.st_blocks  = 0;
  buf.st_atime   = now;
  buf.st_mtime   = now;
  buf.st_ctime   = now;

  return 0;
}

static
int
_getattr(Policy::Func::Search  searchFunc,
         const vector<string> &srcmounts,
         const uint64_t        minfreespace,
         const char           *fusepath,
         struct stat          &buf)
{
  int rv;
  string fullpath;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  fs::path::make(basepaths[0],fusepath,fullpath);

  rv = ::lstat(fullpath.c_str(),&buf);

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    getattr(const char  *fusepath,
            struct stat *st)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _getattr_controlfile(*st);

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _getattr(config.getattr,
                      config.srcmounts,
                      config.minfreespace,
                      fusepath,
                      *st);
    }
  }
}

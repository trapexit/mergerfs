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

#include "fs_mounts.hpp"

#include <cstdio>

#ifdef __linux__
#include <mntent.h>

void
fs::mounts(fs::MountVec &mounts_)
{
  FILE *f;

  f = ::setmntent("/proc/mounts","r");
  if(f == NULL)
    return;

  struct mntent *entry;
  while((entry = ::getmntent(f)) != NULL)
    {
      fs::Mount m;

      m.dir    = entry->mnt_dir;
      m.fsname = entry->mnt_fsname;
      m.type   = entry->mnt_type;
      m.opts   = entry->mnt_opts;

      mounts_.emplace_back(std::move(m));
    }

  ::endmntent(f);
}

#else

void
fs::mounts(fs::MountVec &mounts_)
{
}

#endif

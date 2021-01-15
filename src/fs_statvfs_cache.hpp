/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include <cstdint>

#include <sys/statvfs.h>


namespace fs
{
  uint64_t
  statvfs_cache_timeout(void);
  void
  statvfs_cache_timeout(const uint64_t timeout);

  int
  statvfs_cache(const char     *path,
                struct statvfs *st);

  int
  statvfs_cache_readonly(const std::string &path,
                         bool              *readonly);

  int
  statvfs_cache_spaceavail(const std::string &path,
                           uint64_t          *spaceavail);

  int
  statvfs_cache_spaceused(const std::string &path,
                          uint64_t          *spaceused);
}

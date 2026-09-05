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

#pragma once

#include "base_types.h"

#include <string>

#include <sys/stat.h>

#define FS_COPYFILE_NONE (0)
#define FS_COPYFILE_CLEANUP_FAILURE (1 << 0)

namespace fs
{
  struct CopyFileFlags
  {
    int cleanup_failure:1;
  };

  s64
  copyfile(const int          src_fd,
           const struct stat &src_st,
           const int          dst_fd);

  s64
  copyfile(const int            src_fd,
           const char          *dst_filepath,
           const CopyFileFlags &flags);

  s64
  copyfile(const char          *src_filepath,
           const char          *dst_filepath,
           const CopyFileFlags &flags);

  static
  inline
  s64
  copyfile(const std::string   &src_filepath,
           const std::string   &dst_filepath,
           const CopyFileFlags &flags)
  {
    return fs::copyfile(src_filepath.c_str(),dst_filepath.c_str(),flags);
  }
}

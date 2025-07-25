/*
  ISC License

  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_fstat.hpp"

#define FS_FILE_CHANGED   0
#define FS_FILE_UNCHANGED 1

namespace fs
{
  static
  inline
  int
  file_changed(const int          fd_,
               const struct stat &orig_st_)
  {
    int rv;
    struct stat new_st;

    rv = fs::fstat(fd_,&new_st);
    if(rv < 0)
      return rv;

    if(orig_st_.st_ino != new_st.st_ino)
      return FS_FILE_CHANGED;
    if(orig_st_.st_dev != new_st.st_dev)
      return FS_FILE_CHANGED;
    if(orig_st_.st_size != new_st.st_size)
      return FS_FILE_CHANGED;
    if(orig_st_.st_mtim.tv_sec != new_st.st_mtim.tv_sec)
      return FS_FILE_CHANGED;
    if(orig_st_.st_mtim.tv_nsec != new_st.st_mtim.tv_nsec)
      return FS_FILE_CHANGED;

    return FS_FILE_UNCHANGED;
  }
}

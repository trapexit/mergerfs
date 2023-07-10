/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_mktemp.hpp"
#include "fs_statvfs.hpp"
#include "fs_unlink.hpp"
#include "statvfs_util.hpp"
#include "ugid.hpp"

#include <string>


namespace fs
{
  static
  inline
  bool
  is_mounted_rofs(const std::string path_)
  {
    int rv;
    struct statvfs st;

    rv = fs::statvfs(path_,&st);

    return ((rv == 0) ? StatVFS::readonly(st) : false);
  }

  static
  inline
  bool
  is_rofs(std::string path_)
  {
    ugid::SetRootGuard const ugid;

    int fd;
    std::string tmp_filepath;

    std::tie(fd,tmp_filepath) = fs::mktemp_in_dir(path_,O_WRONLY);
    if(fd < 0)
      return (fd == -EROFS);

    fs::close(fd);
    fs::unlink(tmp_filepath);

    return false;
  }

  static
  inline
  bool
  is_rofs_but_not_mounted_ro(const std::string path_)
  {
    if(fs::is_mounted_rofs(path_))
      return false;

    return fs::is_rofs(path_);
  }
}

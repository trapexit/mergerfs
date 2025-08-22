/*
  ISC License

  Copyright (c) 2017, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_kernel.h"
#include "int_types.h"

#include <string>


namespace symlinkify
{
  static
  inline
  bool
  can_be_symlink(const struct stat &st_,
                 const s64          timeout_)
  {
    if(S_ISDIR(st_.st_mode) ||
       (st_.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)))
      return false;

    const s64 now = ::time(NULL);

    return (((now - st_.st_mtime) > timeout_) &&
            ((now - st_.st_ctime) > timeout_));
  }

  static
  inline
  bool
  can_be_symlink(const struct fuse_statx &st_,
                 const s64                timeout_)
  {
    if(S_ISDIR(st_.mode) ||
       (st_.mode & (S_IWUSR|S_IWGRP|S_IWOTH)))
      return false;

    const s64 now = ::time(NULL);

    return (((now - st_.mtime.tv_sec) > timeout_) &&
            ((now - st_.ctime.tv_sec) > timeout_));
  }

  static
  inline
  void
  convert(const std::string &target_,
          struct stat       *st_)
  {
    st_->st_mode = (((st_->st_mode & ~S_IFMT) | S_IFLNK) | 0777);
    st_->st_size = target_.size();
    st_->st_blocks = 0;
  }

  static
  inline
  void
  convert(const std::string &target_,
          struct fuse_statx *st_)
  {
    st_->mode = (((st_->mode & ~S_IFMT) | S_IFLNK) | 0777);
    st_->size = target_.size();
    st_->blocks = 0;
  }

  static
  inline
  void
  convert_if_can_be_symlink(const std::string &target_,
                            struct stat       *st_,
                            const s64          timeout_)
  {
    if(timeout_ < 0)
      return;
    if(!symlinkify::can_be_symlink(*st_,timeout_))
      return;

    symlinkify::convert(target_,st_);
  }

  static
  inline
  void
  convert_if_can_be_symlink(const std::string &target_,
                            fuse_statx        *st_,
                            const s64          timeout_)
  {
    if(timeout_ < 0)
      return;
    if(!symlinkify::can_be_symlink(*st_,timeout_))
      return;

    symlinkify::convert(target_,st_);
  }
}

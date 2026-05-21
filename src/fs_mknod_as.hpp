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

#include "fs_acl.hpp"
#include "fs_mknod.hpp"
#include "ugid.hpp"


#if defined __linux__
namespace fs
{
  template<typename T>
  static
  inline
  int
  mknod_as(const ugid_t  ugid_,
           const T      &path_,
           mode_t        mode_,
           dev_t         dev_,
           mode_t        umask_)
  {
    if(not fs::acl::dir_has_defaults(path_))
      mode_ &= ~umask_;

    const ugid::SetGuard _(ugid_);

    return fs::mknod(path_,mode_,dev_);
  }
}
#elif defined __FreeBSD__
#include "fs_lchown.hpp"
#include "fs_unlink.hpp"

namespace fs
{
  template<typename T>
  static
  inline
  int
  mknod_as(const ugid_t  ugid_,
           const T      &path_,
           mode_t        mode_,
           dev_t         dev_,
           mode_t        umask_)
  {
    int rv;

    if(not fs::acl::dir_has_defaults(path_))
      mode_ &= ~umask_;

    rv = fs::mknod(path_,mode_,dev_);
    if(rv < 0)
      return rv;

    const int lrv = fs::lchown(path_,ugid_.uid,ugid_.gid);
    if(lrv < 0)
      {
        // Clean up the just-created node so a retry sees -ENOENT
        // instead of -EEXIST and userspace doesn't observe a phantom.
        fs::unlink(path_);
        return lrv;
      }

    return 0;
  }
}
#else
#error "Not Supported!"
#endif

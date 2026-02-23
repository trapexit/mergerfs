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

#include "fs_open.hpp"
#include "ugid.hpp"

// Linux can set ugid because it allows for credentials per
// thread. FreeBSD however does not and must create as root then
// chown. Another option for both platforms would be to create a temp
// file, chown, then rename to target. Unfortunately, depending on if
// the target filesystem is POSIX compliant or uses POSIX / extended
// ACLs it is not possible to know what is best.


#if defined __linux__
namespace fs
{
  static
  inline
  int
  open_as(const ugid_t    ugid_,
          const fs::path &path_,
          const int       flags_,
          const mode_t    mode_)
  {
    const ugid::SetGuard _(ugid_);

    return fs::open(path_,flags_,mode_);
  }
}
#elif defined __FreeBSD__
#include "fs_fchown.hpp"

namespace fs
{
  static
  inline
  int
  open_as(const ugid_t    ugid_,
          const fs::path &path_,
          const int       flags_,
          const mode_t    mode_)
  {
    int rv;

    rv = fs::open(path_,flags_,mode_);
    if(rv < 0)
      return rv;

    fs::fchown(rv,ugid_.uid,ugid_.gid);

    return rv;
  }
}
#else
#error "Not Supported!"
#endif

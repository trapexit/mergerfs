/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <string>

#include <fcntl.h>
#include <sys/stat.h>


namespace fs
{
  static
  inline
  int
  utimensat(const int              dirfd_,
            const char            *pathname_,
            const struct timespec  times_[2],
            const int              flags_)
  {
    return ::utimensat(dirfd_,pathname_,times_,flags_);
  }

  static
  inline
  int
  utimensat(const int              dirfd_,
            const std::string     &pathname_,
            const struct timespec  times_[2],
            const int              flags_)
  {
    return fs::utimensat(dirfd_,pathname_.c_str(),times_,flags_);
  }
}

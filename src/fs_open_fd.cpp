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

#include "fs_open_fd.hpp"

#if defined(__linux__)
#include "fmt/core.h"
#include "fs_openat.hpp"
#include "fatal.hpp"
#include "procfs.hpp"

int
fs::open_fd(const int fd_,
            const int flags_)
{
  if(procfs::PROC_SELF_FD_FD <= 0)
    fatal::abort("procfs::PROC_SELF_FD_FD must be > 0");

  const int flags = (flags_ & ~O_NOFOLLOW);

  return fs::openat(procfs::PROC_SELF_FD_FD,
                    fmt::format("{}",fd_),
                    flags);
}
#elif defined(__FreeBSD__)
#include "fs_openat.hpp"

int
fs::open_fd(const int fd_,
            const int flags_)
{
  const int flags = ((flags_ | O_EMPTY_PATH) & ~O_NOFOLLOW);

  return fs::openat(fd_,"",flags);
}
#else
#error "fs::open_fd() not supported on platform"
#endif

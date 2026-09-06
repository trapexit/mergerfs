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

#include "pipe_max_size.hpp"

#include "scope_guard/scope_guard.hpp"

#include <array>
#include <charconv>

#include <fcntl.h>
#include <unistd.h>

static const char PIPE_MAX_SIZE_FILEPATH[] = "/proc/sys/fs/pipe-max-size";

bool
pipe_max_size::read(u32 &out_)
{
  int fd = ::open(PIPE_MAX_SIZE_FILEPATH,O_RDONLY|O_CLOEXEC);
  if(fd == -1)
    return false;
  DEFER { ::close(fd); };

  std::array<char,16> buf;
  ssize_t nr = ::read(fd,buf.data(),buf.size());
  if(nr <= 0)
    return false;

  unsigned long v;
  auto [end,ec] = std::from_chars(buf.data(),buf.data() + nr,v);
  if(ec != std::errc())
    return false;

  /* Clamp rather than treat as a read failure: a legitimately
     huge sysctl (e.g. an admin-set multi-GiB pipe-max-size,
     written verbatim as a u64 by the pipe-max-size= option)
     must not be mistaken for "couldn't read it" and fall back
     to a small default in the caller - that fallback can then
     silently shrink the admin's explicit setting when the
     caller reacts to a seemingly-tiny sysctl. */
  out_ = (v > UINT32_MAX) ? UINT32_MAX : (u32)v;

  return true;
}

bool
pipe_max_size::write(cu64 v_)
{
  int fd = ::open(PIPE_MAX_SIZE_FILEPATH,O_WRONLY|O_CLOEXEC);
  if(fd == -1)
    return false;
  DEFER { ::close(fd); };

  std::array<char,32> buf;
  auto [end,ec] = std::to_chars(buf.data(),buf.data() + buf.size(),(u64)v_);
  if(ec != std::errc())
    return false;

  ssize_t len = (end - buf.data());
  ssize_t wr  = ::write(fd,buf.data(),len);

  return (wr == len);
}

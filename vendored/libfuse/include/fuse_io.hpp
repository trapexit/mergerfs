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

#include <cstddef>
#include <shared_mutex>
#include <sys/types.h>

/* A borrowed, descriptor-referenced region of a branch file that a read
   reply can splice into /dev/fuse without the data crossing userspace.
   The filesystem fills it; libfuse never owns or closes the fd.

   The reply splices from fd several times, so the filesystem may park
   a shared lock in lock to keep fd bound to the same file until every
   read from fd is done (staged into the reply). The reply path
   releases it as soon as that staging finishes, whether or not the
   reply is then successfully delivered: fd is not read again either
   way, and once the READ is answered the kernel may send RELEASE, so
   the filesystem may free what the lock lives in. A fallback reached
   instead DURING staging (fd itself declined or ran out of data)
   leaves the lock held; it is released when the region goes out of
   scope, before any fallback read. */
struct FileRegion
{
  int    fd     = -1;
  off_t  offset = 0;
  size_t size   = 0;
  std::shared_lock<std::shared_mutex> lock;
};

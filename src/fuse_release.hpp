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

/*
  FUSE RELEASE INTERFACE
  ======================

  This header defines the two release() function overloads used for closing
  open files. There are two variants because release can be initiated from
  different contexts:

  1. release(ctx, ffi) - Standard FUSE release callback
     Called by libfuse when a file is closed. Extracts the FileInfo from
     the fuse_file_info_t handle and calls the internal release function.

  2. release(nodeid, fi) - Internal release function
     Called internally when we need to release a file that failed to open
     properly (e.g., in fuse_open.cpp when _open_fd fails after finding an
     existing open). This allows the same cleanup logic to be used for both
     normal closes and error cleanup.

  The two-phase cleanup mechanism is implemented in fuse_release.cpp. See
  that file for detailed documentation on the reference counting and
  canonical vs per-handle ownership rules.

  See commit 446e9a7b for the complete rationale behind this design.
*/

#pragma once

#include "base_types.h"
#include "fuse.h"


class FileInfo;

namespace FUSE
{
  int
  release(const fuse_req_ctx_t   *ctx_,
          const fuse_file_info_t *ffi_);

  void
  release(cu64             nodeid,
          FileInfo * const fi = nullptr);
}

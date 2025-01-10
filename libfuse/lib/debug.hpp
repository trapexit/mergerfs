/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

void debug_fuse_open_out(const uint64_t              unique,
                         const struct fuse_open_out *arg,
                         const uint64_t              argsize);
void debug_fuse_init_in(const struct fuse_init_in *arg);
void debug_fuse_init_out(const uint64_t              unique,
                         const struct fuse_init_out *arg,
                         const uint64_t              argsize);
void debug_fuse_entry_out(const uint64_t               unique,
                          const struct fuse_entry_out *arg,
                          const uint64_t               argsize);
void debug_fuse_attr_out(const uint64_t              unique,
                         const struct fuse_attr_out *arg,
                         const uint64_t              argsize);
void debug_fuse_entry_open_out(const uint64_t               unique,
                               const struct fuse_entry_out *earg,
                               const struct fuse_open_out  *oarg);
void debug_fuse_readlink(const uint64_t  unique,
                         const char     *linkname);
void debug_fuse_write_out(const uint64_t               unique,
                          const struct fuse_write_out *arg);
void debug_fuse_statfs_out(const uint64_t                unique,
                           const struct fuse_statfs_out *arg);
void debug_fuse_getxattr_out(const uint64_t                  unique,
                             const struct fuse_getxattr_out *arg);
void debug_fuse_lk_out(const uint64_t            unique,
                       const struct fuse_lk_out *arg);
void debug_fuse_bmap_out(const uint64_t              unique,
                         const struct fuse_bmap_out *arg);
void debug_fuse_in_header(const struct fuse_in_header *hdr);

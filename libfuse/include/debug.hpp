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

#include <memory>
#include <string>


int fuse_debug_set_output(const std::string &filepath);

void fuse_debug_open_out(const uint64_t              unique,
                         const struct fuse_open_out *arg,
                         const uint64_t              argsize);
void fuse_debug_init_in(const struct fuse_init_in *arg);
void fuse_debug_init_out(const uint64_t              unique,
                         const struct fuse_init_out *arg,
                         const uint64_t              argsize);
void fuse_debug_entry_out(const uint64_t               unique,
                          const struct fuse_entry_out *arg,
                          const uint64_t               argsize);
void fuse_debug_attr_out(const uint64_t              unique,
                         const struct fuse_attr_out *arg,
                         const uint64_t              argsize);
void fuse_debug_entry_open_out(const uint64_t               unique,
                               const struct fuse_entry_out *earg,
                               const struct fuse_open_out  *oarg);
void fuse_debug_readlink(const uint64_t  unique,
                         const char     *linkname);
void fuse_debug_write_out(const uint64_t               unique,
                          const struct fuse_write_out *arg);
void fuse_debug_statfs_out(const uint64_t                unique,
                           const struct fuse_statfs_out *arg);
void fuse_debug_getxattr_out(const uint64_t                  unique,
                             const struct fuse_getxattr_out *arg);
void fuse_debug_lk_out(const uint64_t            unique,
                       const struct fuse_lk_out *arg);
void fuse_debug_bmap_out(const uint64_t              unique,
                         const struct fuse_bmap_out *arg);
void fuse_debug_statx_out(const uint64_t               unique,
                          const struct fuse_statx_out *arg);
void fuse_debug_data_out(const uint64_t unique,
                         const size_t   size);
void fuse_debug_ioctl_out(const uint64_t               unique,
                          const struct fuse_ioctl_out *arg);
void fuse_debug_poll_out(const uint64_t              unique,
                         const struct fuse_poll_out *arg);
void fuse_debug_in_header(const struct fuse_in_header *hdr);
void fuse_debug_out_header(const struct fuse_out_header *hdr);

std::string fuse_debug_init_flag_name(const uint64_t);

void fuse_syslog_fuse_init_in(const struct fuse_init_in *arg);
void fuse_syslog_fuse_init_out(const struct fuse_init_out *arg);

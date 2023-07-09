/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_msgbuf.h"
#include "extern_c.h"

EXTERN_C_BEGIN

void     msgbuf_set_bufsize(const uint32_t size);
uint64_t msgbuf_get_bufsize();

fuse_msgbuf_t *msgbuf_alloc();
fuse_msgbuf_t *msgbuf_alloc_page_aligned();
void           msgbuf_free(fuse_msgbuf_t *msgbuf);

void           msgbuf_gc();
void           msgbuf_gc_10percent();

uint64_t       msgbuf_alloc_count();
uint64_t       msgbuf_avail_count();

void           msgbuf_page_align(fuse_msgbuf_t *msgbuf);
void           msgbuf_write_align(fuse_msgbuf_t *msgbuf);

EXTERN_C_END

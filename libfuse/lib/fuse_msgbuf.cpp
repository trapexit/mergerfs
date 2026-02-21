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

#include "fuse_msgbuf.hpp"

#include "fuse.h"
#include "fuse_kernel.h"
#include "objpool.hpp"

#include "fmt/core.h"

#include <unistd.h>

#include <cassert>
#include <cstdlib>


static u32 g_pagesize = 0;
static u64 g_bufsize  = 0;

static
__attribute__((constructor))
void
_constructor()
{
  long pagesize = sysconf(_SC_PAGESIZE);

  assert(pagesize > 0);
  g_pagesize = pagesize;

  assert((sizeof(struct fuse_in_header) + sizeof(struct fuse_write_in))
         < g_pagesize);

  msgbuf_set_bufsize(FUSE_DEFAULT_MAX_MAX_PAGES);
}

struct PageAlignedAllocator
{
  void*
  allocate(size_t size_, size_t align_)
  {
    void *buf = NULL;
    int rv = posix_memalign(&buf, align_, size_);
    return (rv == 0) ? buf : NULL;
  }

  void
  deallocate(void *ptr_, size_t /*size_*/, size_t /*align_*/)
  {
    free(ptr_);
  }
};

struct ShouldPoolMsgbuf
{
  bool operator()(const fuse_msgbuf_t *msgbuf) const noexcept
  {
    return msgbuf->size == (g_bufsize - g_pagesize);
  }
};

static ObjPool<fuse_msgbuf_t, PageAlignedAllocator, ShouldPoolMsgbuf> g_msgbuf_pool;

static
void
_msgbuf_page_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_pagesize;
  msgbuf_->size  = (g_bufsize - g_pagesize);
}

static
void
_msgbuf_write_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_pagesize;
  msgbuf_->mem  -= sizeof(struct fuse_in_header);
  msgbuf_->mem  -= sizeof(struct fuse_write_in);
  msgbuf_->size  = (g_bufsize - g_pagesize);
}

typedef void (*msgbuf_setup_func_t)(fuse_msgbuf_t*);

static
fuse_msgbuf_t*
_msgbuf_alloc(msgbuf_setup_func_t setup_func_)
{
  fuse_msgbuf_t *msgbuf;

  msgbuf = g_msgbuf_pool.alloc(g_bufsize);
  if(msgbuf == NULL)
    return NULL;

  setup_func_(msgbuf);

  return msgbuf;
}

fuse_msgbuf_t*
msgbuf_alloc()
{
  return _msgbuf_alloc(_msgbuf_write_align);
}

fuse_msgbuf_t*
msgbuf_alloc_page_aligned()
{
  return _msgbuf_alloc(_msgbuf_page_align);
}

void
msgbuf_free(fuse_msgbuf_t *msgbuf_)
{
  g_msgbuf_pool.free(msgbuf_, g_bufsize);
}

u64
msgbuf_get_bufsize()
{
  return g_bufsize;
}

u32
msgbuf_get_pagesize()
{
  return g_pagesize;
}

void
msgbuf_set_bufsize(const u64 size_in_pages_)
{
  g_bufsize = ((size_in_pages_ + 2) * g_pagesize);
}

u64
msgbuf_alloc_count()
{
  return g_msgbuf_pool.size();
}

u64
msgbuf_avail_count()
{
  return g_msgbuf_pool.size();
}

void
msgbuf_gc()
{
  g_msgbuf_pool.gc();
}

void
msgbuf_clear()
{
  g_msgbuf_pool.clear();
}

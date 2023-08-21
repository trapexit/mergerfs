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

#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>


static std::uint32_t g_PAGESIZE = 0;
static std::uint32_t g_BUFSIZE  = 0;

static std::atomic<std::uint_fast64_t> g_MSGBUF_ALLOC_COUNT;

static std::mutex g_MUTEX;
static std::vector<fuse_msgbuf_t*> g_MSGBUF_STACK;

uint64_t
msgbuf_get_bufsize()
{
  return g_BUFSIZE;
}

void
msgbuf_set_bufsize(const uint32_t size_in_pages_)
{
  g_BUFSIZE = ((size_in_pages_ + 1) * g_PAGESIZE);
}

void
msgbuf_page_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_PAGESIZE;
  msgbuf_->size  = (g_BUFSIZE - g_PAGESIZE);
}

void
msgbuf_write_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_PAGESIZE;
  msgbuf_->mem  -= sizeof(struct fuse_in_header);
  msgbuf_->mem  -= sizeof(struct fuse_write_in);
  msgbuf_->size  = (g_BUFSIZE - g_PAGESIZE);
}

static
__attribute__((constructor))
void
msgbuf_constructor()
{
  g_PAGESIZE = sysconf(_SC_PAGESIZE);
  // FUSE_MAX_MAX_PAGES for payload + 1 for message header
  msgbuf_set_bufsize(FUSE_MAX_MAX_PAGES + 1);
}

static
__attribute__((destructor))
void
msgbuf_destroy()
{

}

static
void*
page_aligned_malloc(const uint64_t size_)
{
  int rv;
  void *buf = NULL;

  rv = posix_memalign(&buf,g_PAGESIZE,size_);
  if(rv != 0)
    return NULL;

  return buf;
}

typedef void (*msgbuf_setup_func_t)(fuse_msgbuf_t*);

static
fuse_msgbuf_t*
_msgbuf_alloc(msgbuf_setup_func_t setup_func_)
{
  fuse_msgbuf_t *msgbuf;

  g_MUTEX.lock();
  if(g_MSGBUF_STACK.empty())
    {
      g_MUTEX.unlock();

      msgbuf = (fuse_msgbuf_t*)page_aligned_malloc(g_BUFSIZE);
      if(msgbuf == NULL)
        return NULL;

      g_MSGBUF_ALLOC_COUNT.fetch_add(1,std::memory_order_relaxed);
    }
  else
    {
      msgbuf = g_MSGBUF_STACK.back();
      g_MSGBUF_STACK.pop_back();
      g_MUTEX.unlock();
    }

  setup_func_(msgbuf);

  return msgbuf;
}

// Offset the memory so write request payload will be placed on page
// boundry so O_DIRECT can work. No impact on other message types
// except for `read` which will require using `msgbuf_page_align`.
fuse_msgbuf_t*
msgbuf_alloc()
{
  return _msgbuf_alloc(msgbuf_write_align);
}

fuse_msgbuf_t*
msgbuf_alloc_page_aligned()
{
  return _msgbuf_alloc(msgbuf_page_align);
}

static
void
msgbuf_destroy(fuse_msgbuf_t *msgbuf_)
{
  //  free(msgbuf_->mem);
  free(msgbuf_);
}

void
msgbuf_free(fuse_msgbuf_t *msgbuf_)
{
  std::lock_guard<std::mutex> lck(g_MUTEX);

  if(msgbuf_->size != (g_BUFSIZE - g_PAGESIZE))
    {
      msgbuf_destroy(msgbuf_);
      g_MSGBUF_ALLOC_COUNT.fetch_sub(1,std::memory_order_relaxed);
      return;
    }

  g_MSGBUF_STACK.emplace_back(msgbuf_);

}

uint64_t
msgbuf_alloc_count()
{
  return g_MSGBUF_ALLOC_COUNT;
}

uint64_t
msgbuf_avail_count()
{
  std::lock_guard<std::mutex> lck(g_MUTEX);

  return g_MSGBUF_STACK.size();
}

void
msgbuf_gc_10percent()
{
  std::vector<fuse_msgbuf_t*> togc;

  {
    std::size_t size;
    std::size_t ten_percent;

    std::lock_guard<std::mutex> lck(g_MUTEX);

    size        = g_MSGBUF_STACK.size();
    ten_percent = (size / 10);

    for(std::size_t i = 0; i < ten_percent; i++)
      {
        togc.push_back(g_MSGBUF_STACK.back());
        g_MSGBUF_STACK.pop_back();
      }
  }

  for(auto msgbuf : togc)
    {
      msgbuf_destroy(msgbuf);
      g_MSGBUF_ALLOC_COUNT.fetch_sub(1,std::memory_order_relaxed);
    }
}

void
msgbuf_gc()
{
  std::vector<fuse_msgbuf_t*> oldstack;

  {
    std::lock_guard<std::mutex> lck(g_MUTEX);
    oldstack.swap(g_MSGBUF_STACK);
  }

  for(auto msgbuf: oldstack)
    {
      msgbuf_destroy(msgbuf);
      g_MSGBUF_ALLOC_COUNT.fetch_sub(1,std::memory_order_relaxed);
    }
}

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

#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>


static std::uint32_t g_PAGESIZE = 0;
static std::uint32_t g_BUFSIZE  = 0;

static std::mutex g_MUTEX;
static std::vector<fuse_msgbuf_t*> g_MSGBUF_STACK;

static
__attribute__((constructor))
void
msgbuf_constructor()
{
  g_PAGESIZE = sysconf(_SC_PAGESIZE);
  // +2 because to do O_DIRECT we need to offset the buffer to align
  g_BUFSIZE  = (g_PAGESIZE * (FUSE_MAX_MAX_PAGES + 2));
}

static
__attribute__((destructor))
void
msgbuf_destroy()
{
  // TODO: cleanup?
}

uint32_t
msgbuf_get_bufsize()
{
  return g_BUFSIZE;
}

void
msgbuf_set_bufsize(const uint32_t size_in_pages_)
{
  g_BUFSIZE = (size_in_pages_ * g_PAGESIZE);
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

fuse_msgbuf_t*
msgbuf_alloc()
{
  std::lock_guard<std::mutex> lck(g_MUTEX);
  fuse_msgbuf_t *msgbuf;

  if(g_MSGBUF_STACK.empty())
    {
      msgbuf = (fuse_msgbuf_t*)malloc(sizeof(fuse_msgbuf_t));
      if(msgbuf == NULL)
        return NULL;

      msgbuf->mem = (char*)page_aligned_malloc(g_BUFSIZE);
      if(msgbuf->mem == NULL)
        {
          free(msgbuf);
          return NULL;
        }

      msgbuf->size = g_BUFSIZE;
    }
  else
    {
      msgbuf = g_MSGBUF_STACK.back();
      g_MSGBUF_STACK.pop_back();
    }

  return msgbuf;
}

void
msgbuf_free(fuse_msgbuf_t *msgbuf_)
{
  std::lock_guard<std::mutex> lck(g_MUTEX);

  if(msgbuf_->size != g_BUFSIZE)
    {
      free(msgbuf_->mem);
      free(msgbuf_);
      return;
    }

  g_MSGBUF_STACK.emplace_back(msgbuf_);
}

uint64_t
msgbuf_alloc_count()
{
  std::lock_guard<std::mutex> lck(g_MUTEX);

  return g_MSGBUF_STACK.size();
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
      free(msgbuf->mem);
      free(msgbuf);
    }
}

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

#include "fuse_msgbuf.h"

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <mutex>
#include <stack>


static std::size_t g_BUFSIZE = (1024 * 1024 * 2);

static std::mutex g_MUTEX;
static std::stack<fuse_msgbuf_t*> g_MSGBUF_STACK;

static
__attribute__((destructor))
void
msgbuf_destroy()
{
  // TODO: cleanup?
}

std::size_t
msgbuf_bufsize()
{
  return g_BUFSIZE;
}

void
msgbuf_bufsize(const std::size_t size_)
{
  g_BUFSIZE = size_;
}

fuse_msgbuf_t*
msgbuf_alloc()
{
  int rv;
  fuse_msgbuf_t *msgbuf;

  g_MUTEX.lock();
  if(g_MSGBUF_STACK.empty())
    {
      g_MUTEX.unlock();

      msgbuf = (fuse_msgbuf_t*)malloc(sizeof(fuse_msgbuf_t));
      if(msgbuf == NULL)
        return NULL;

      rv = pipe(msgbuf->pipefd);
      assert(rv == 0);
      rv = fcntl(msgbuf->pipefd[0],F_SETPIPE_SZ,g_BUFSIZE);
      assert(rv > 0);
      msgbuf->mem  = (char*)malloc(rv);
      msgbuf->size = rv;
      msgbuf->used = 0;
    }
  else
    {
      msgbuf = g_MSGBUF_STACK.top();
      g_MSGBUF_STACK.pop();
      g_MUTEX.unlock();
    }

  return msgbuf;
}

fuse_msgbuf_t*
msgbuf_alloc_memonly()
{
  fuse_msgbuf_t *msgbuf;

  g_MUTEX.lock();
  if(g_MSGBUF_STACK.empty())
    {
      g_MUTEX.unlock();

      msgbuf = (fuse_msgbuf_t*)malloc(sizeof(fuse_msgbuf_t));
      if(msgbuf == NULL)
        return NULL;

      msgbuf->pipefd[0] = -1;
      msgbuf->pipefd[1] = -1;
      msgbuf->mem  = (char*)malloc(g_BUFSIZE);
      msgbuf->size = g_BUFSIZE;
      msgbuf->used = 0;
    }
  else
    {
      msgbuf = g_MSGBUF_STACK.top();
      g_MSGBUF_STACK.pop();
      g_MUTEX.unlock();
    }

  return msgbuf;
}

void
msgbuf_free(fuse_msgbuf_t *msgbuf_)
{
  std::lock_guard<std::mutex> lck(g_MUTEX);

  g_MSGBUF_STACK.push(msgbuf_);
}

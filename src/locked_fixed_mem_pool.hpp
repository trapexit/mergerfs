/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fixed_mem_pool.hpp"

#include <pthread.h>

template<size_t SIZE>
class LockedFixedMemPool
{
public:
  LockedFixedMemPool()
  {
    pthread_mutex_init(&_mutex,NULL);
  }

  ~LockedFixedMemPool()
  {
    pthread_mutex_destroy(&_mutex);
  }

public:
  void*
  alloc(void)
  {
    void *mem;

    pthread_mutex_lock(&_mutex);
    mem = _fmp.alloc();
    pthread_mutex_unlock(&_mutex);

    return mem;
  }

  void
  free(void *mem_)
  {
    pthread_mutex_lock(&_mutex);
    _fmp.free(mem_);
    pthread_mutex_unlock(&_mutex);
  }

  uint64_t
  size(void)
  {
    return _fmp.size();
  }

private:
  FixedMemPool<SIZE> _fmp;
  pthread_mutex_t    _mutex;
};

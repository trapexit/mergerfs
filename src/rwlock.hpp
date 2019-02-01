/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <pthread.h>

namespace rwlock
{
  class ReadGuard
  {
  public:
    ReadGuard(pthread_rwlock_t *lock_)
      : _lock(lock_)
    {
      pthread_rwlock_rdlock(_lock);
    }

    ~ReadGuard()
    {
      pthread_rwlock_unlock(_lock);
    }

  private:
    ReadGuard();

  private:
    pthread_rwlock_t *_lock;
  };

  class WriteGuard
  {
  public:
    WriteGuard(pthread_rwlock_t *lock_)
      : _lock(lock_)
    {
      pthread_rwlock_wrlock(_lock);
    }

    ~WriteGuard()
    {
      pthread_rwlock_unlock(_lock);
    }

  private:
    WriteGuard();

  private:
    pthread_rwlock_t *_lock;
  };
}

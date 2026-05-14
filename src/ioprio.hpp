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

#pragma once

#include <atomic>

#include <sys/types.h>


namespace ioprio
{
  // Exposed so the disabled-path check in SetFrom can inline into
  // every fuse_read/fuse_write as an atomic load + branch, with no
  // function overhead.
  extern std::atomic<bool> _enabled;

  [[gnu::always_inline]]
  inline
  bool
  enabled()
  {
    return _enabled.load(std::memory_order_relaxed);
  }

  void enable(const bool);

  int get(const int who);
  int set(const int who, const int ioprio);

  struct SetFrom
  {
    static thread_local int thread_prio;

    [[gnu::always_inline]]
    inline
    SetFrom(const pid_t pid_)
    {
      if(ioprio::enabled())
        _slow_apply(pid_);
    }

  private:
    void _slow_apply(const pid_t);
  };
};

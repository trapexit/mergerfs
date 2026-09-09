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

#include "base_types.h"

#include <atomic>
#include <mutex>
#include <string>


namespace qos
{
  // Sentinel meaning "leave the worker thread's current value alone".
  constexpr int UNSET = INT32_MIN;

  namespace ioprio
  {
    // Mirrors the kernel's linux/ioprio.h encoding. Redefined here so
    // mergerfs does not depend on a UAPI header that has moved
    // between kernel versions.
    constexpr int CLASS_SHIFT = 13;
    constexpr int PRIO_MASK   = (1 << CLASS_SHIFT) - 1;

    constexpr int CLASS_NONE = 0;
    constexpr int CLASS_RT   = 1;
    constexpr int CLASS_BE   = 2;
    constexpr int CLASS_IDLE = 3;

    constexpr
    int
    value(const int class_,
          const int data_)
    {
      return ((class_ << CLASS_SHIFT) | data_);
    }

    constexpr
    int
    to_class(const int value_)
    {
      return (value_ >> CLASS_SHIFT);
    }

    constexpr
    int
    to_data(const int value_)
    {
      return (value_ & PRIO_MASK);
    }

    std::string to_string(const int value);

    // Parses "rt:N", "be:N", "idle" and "none". Returns 0 on success
    // and -EINVAL otherwise.
    int from_string(const std::string_view, int *value);
  }

  // A QoS class is a named bundle of scheduling parameters plus the
  // token bucket and counters belonging to every request matched to
  // it.
  //
  // The mutable atomics are the only members written after a ruleset
  // is published; the descriptive members are immutable for the life
  // of the object, which is what lets the classifier hand out bare
  // pointers to it.
  class Class
  {
  public:
    Class(std::string name_)
      : name(std::move(name_))
    {
    }

  public:
    std::string name;

    int ioprio = qos::UNSET;
    int nice   = qos::UNSET;

    // 0 == unlimited. `burst` is the bucket depth and defaults to one
    // second of `rate` when a rate is set.
    u64 rate  = 0;
    u64 burst = 0;

  public:
    // Token bucket, guarded by `bucket_mutex`. `tokens` is in bytes
    // and `refilled` is a CLOCK_MONOTONIC nanosecond stamp. All three
    // are only touched when `rate` is non-zero, so classes without a
    // rate never take the lock.
    mutable std::mutex       bucket_mutex;
    mutable u64              tokens{0};
    mutable u64              refilled{0};

  public:
    // Counters, reported via the qos.stats config key.
    mutable std::atomic<u64> requests{0};
    mutable std::atomic<u64> bytes{0};
    mutable std::atomic<u64> throttled{0};
    mutable std::atomic<u64> throttled_ns{0};
    mutable std::atomic<u64> passed{0};
  };
}

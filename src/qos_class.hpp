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
#include <memory>
#include <unordered_map>


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

  // One token bucket. A class owns one of these per resource it is
  // limited against -- normally one per branch, so a rate is a
  // per-device rate rather than a pool-wide one. Two players reading
  // two different disks do not compete for the same allowance.
  struct Bucket
  {
    std::mutex mutex;
    u64        tokens   = 0;
    u64        refilled = 0;
    bool       primed   = false;
  };

  // The feedback state for one resource (one branch/spindle).
  //
  // This is what makes the policy adaptive rather than a fixed cap:
  // bulk traffic runs unrestricted on a disk nobody is streaming from,
  // and only gives way -- gradually, and never to a standstill -- once
  // a protected class on that same disk starts to suffer.
  struct Governor
  {
    std::mutex mutex;

    // When a protected class last issued I/O here. Bulk classes read
    // this to answer "is anyone playing off this spindle right now?"
    u64 protected_at = 0;

    // When a yielding class last issued I/O here. Pressure only rises
    // while both this and protected_at are recent -- that is the
    // definition of contention.
    u64 yielding_at = 0;

    // Exponentially weighted mean service time of protected requests,
    // and the quietest mean seen so far, which stands in for "what
    // this disk does when nothing is fighting it".
    u64 latency_ewma = 0;
    u64 latency_base = 0;

    // 0.0 == no backoff, 1.0 == full backoff. Moved multiplicatively
    // up on distress and additively down on recovery, so it reacts
    // fast to a stutter and returns slowly enough not to oscillate.
    double pressure = 0.0;

    u64 updated_at = 0;

    // Counters for qos.stats.
    u64 distress_events = 0;
  };

  // A QoS class is a named bundle of scheduling parameters plus the
  // token buckets and counters belonging to every request matched to
  // it.
  //
  // The descriptive members are immutable once a ruleset is
  // published, which is what lets the classifier hand out bare
  // pointers to it. Everything mutable is either atomic or guarded.
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

    // A rate is either absolute (`rate` bytes/sec) or a share of the
    // serving resource's measured capacity (`pct` percent). Both zero
    // means unlimited.
    u64 rate  = 0;
    u32 pct   = 0;
    // Bucket depth. Zero means "one second of whatever the rate
    // resolves to", which cannot be precomputed for a percentage.
    u64 burst = 0;

    // Latency of this class's requests is the signal the governor
    // controls against, and it is never itself throttled.
    bool protect = false;

    // Never delayed, and never a control signal.
    //
    // This exists for the services playback *synchronously waits on*
    // rather than playback itself -- Plex's EasyAudioEncoder is the
    // canonical case: a transcode of EAC3/TrueHD/DTS audio writes to
    // EAE and blocks until it answers. Slowing such a helper slows the
    // stream that is waiting on it, so the "bulk" it appears to be
    // doing must be left alone. Getting this wrong is a priority
    // inversion: the machinery meant to protect playback stalls it.
    bool critical = false;

    // True when this class must never be delayed for any reason.
    bool immune() const { return (protect || critical); }

    // How strongly this class gives way as pressure rises, 0-100.
    // 0 never yields; 100 yields its whole allowance at full
    // pressure. This is the priority ladder: downloads yield hardest,
    // a player's background work yields some, playback yields none.
    u32 yield = 0;

    // Never back off below this, so a yielding class is slowed rather
    // than stopped. Either absolute bytes/sec or a percentage of the
    // resource's capacity.
    u64 floor     = 0;
    u32 floor_pct = 0;

    bool limited() const { return ((rate != 0) || (pct != 0)); }
    bool adaptive() const { return (yield != 0); }

  public:
    // Buckets are created on first use and keyed by resource name
    // (a branch path, or "pool" when the branch is unknown). Seven
    // branches means seven entries, so a plain map under a mutex is
    // cheaper than anything cleverer.
    mutable std::mutex                           buckets_mutex;
    mutable std::unordered_map<std::string,
                               std::unique_ptr<Bucket>> buckets;

    Bucket *bucket_for(const std::string &resource) const;

  public:
    // Counters, reported via the qos.stats config key.
    mutable std::atomic<u64> requests{0};
    mutable std::atomic<u64> bytes{0};
    mutable std::atomic<u64> throttled{0};
    mutable std::atomic<u64> throttled_ns{0};
    mutable std::atomic<u64> passed{0};
  };
}

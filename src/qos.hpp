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

/*
  Per-client quality of service.

  mergerfs is a single daemon serving every process that touches the
  pool, so from the kernel's point of view all pool I/O is issued by
  one cgroup and one set of threads. Block layer priority
  (ionice/BFQ, cgroup io.weight) therefore cannot tell a media player
  apart from a torrent client: they are flattened into the same
  issuer.

  This restores the distinction inside the daemon. Each request is
  attributed to the calling process, matched against a ruleset, and
  the worker thread is given the ioprio and nice value of the class it
  matched for the duration of the request. Optionally the class is
  also rate limited.

  Compared to the `proxy_ioprio` option, which copies whatever ioprio
  the caller happens to have, the policy lives in one place and does
  not depend on every client being correctly ioniced by whoever
  started it -- which matters most for clients that respawn workers,
  or that live in a container the daemon does not control.
 */

#pragma once

#include "qos_class.hpp"
#include "qos_rules.hpp"

#include "fuse_req_ctx.h"

#include <atomic>
#include <string>


namespace qos
{
  // Exposed so the disabled-path check inlines into every
  // fuse_read/fuse_write as an atomic load and a branch.
  extern std::atomic<bool> _enabled;

  [[gnu::always_inline]]
  inline
  bool
  enabled()
  {
    return _enabled.load(std::memory_order_relaxed);
  }

  void enable(const bool);

  // Replaces the active ruleset. Safe to call while requests are in
  // flight.
  void set_ruleset(RuleSet::Ptr);
  RuleSet::Ptr ruleset();

  // Loads and installs a ruleset from `path`. On failure the existing
  // ruleset is left untouched and `err` describes the problem.
  int load_file(const std::string &path, std::string *err);

  // Number of process threads allowed to be sleeping in a throttle at
  // once, and the longest any single request may be delayed.
  //
  // Both exist to bound the damage throttling can do. mergerfs hands
  // requests from its read threads to a *bounded* process thread
  // queue, so a sleeping process thread is a process thread not
  // serving anyone -- and once the queue backs up, requests of every
  // class queue behind it. Rather than let a rate limit turn into a
  // pool-wide stall, a request that would exceed either bound is
  // allowed through unthrottled and counted in the `passed` stat.
  extern std::atomic<int> max_sleepers;
  extern std::atomic<u64> max_sleep_ns;

  // Path most recently loaded by load_file(), or empty.
  std::string rules_path();

  std::string stats();
  void        reset_stats();

  // Charges `bytes` to the class and, if that class is over its rate,
  // sleeps for as long as the bounds above permit. Called with the
  // size the caller asked for, before the I/O is issued.
  void throttle(const Class *, const u64 bytes);

  // Classifies the calling process and applies its class to this
  // thread. The thread keeps those settings until another request
  // changes them, so nothing is restored on destruction -- FUSE
  // worker threads do nothing between requests worth protecting, and
  // restoring would double the syscalls on the hot path.
  class Apply
  {
  public:
    [[gnu::always_inline]]
    inline
    Apply(const fuse_req_ctx_t *ctx_)
    {
      if(qos::enabled())
        _slow_apply(ctx_);
    }

    // nullptr when QoS is off or the ruleset cannot change anything.
    const Class *cls() const { return _cls; }

  private:
    void _slow_apply(const fuse_req_ctx_t *);

  private:
    const Class *_cls = nullptr;
  };
}

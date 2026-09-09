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

#include "qos.hpp"

#include "ioprio.hpp"
#include "procfs.hpp"

#include "fmt/core.h"

#include <errno.h>
#include <sys/resource.h>
#include <time.h>

#include <fstream>
#include <mutex>
#include <sstream>

using qos::Class;
using qos::RuleSet;

namespace qos
{
  std::atomic<bool> _enabled{false};

  // Deliberately small. Each sleeping process thread is one fewer
  // thread draining the shared request queue, so the ceiling has to
  // stay well under process-thread-count. See qos.hpp.
  std::atomic<int> max_sleepers{2};
  std::atomic<u64> max_sleep_ns{50ULL * 1000 * 1000};
}

namespace
{
  constexpr u64 NS_PER_SEC = 1000000000ULL;

  // How long a pid's classification is trusted before it is looked up
  // again. Short enough that a pid recycled onto a different class is
  // corrected quickly, long enough that a streaming reader is not
  // re-reading /proc on every request.
  constexpr u64 CACHE_TTL_NS = 10 * NS_PER_SEC;

  constexpr int CACHE_SIZE = 8;

  // Ceiling on how far into the future a class's refill point may be
  // pushed. Without it a burst that cannot be slept off -- because
  // sleeps are capped, or too many threads are already sleeping --
  // would leave the class paying down debt long after the burst
  // ended, throttling traffic that did nothing wrong.
  constexpr u64 MAX_DEBT_NS = 2 * NS_PER_SEC;

  std::mutex          g_mutex;
  std::atomic<u64>    g_generation{1};
  std::atomic<int>    g_sleepers{0};
  std::string         g_path;

  // Function-local static so the first ruleset is built on demand
  // rather than during static initialisation, where its ordering
  // against Config's constructor would not be defined.
  RuleSet::Ptr &
  _ruleset()
  {
    static RuleSet::Ptr rs = RuleSet::make_default();
    return rs;
  }

  struct CacheEntry
  {
    u32          pid    = 0;
    u64          expire = 0;
    const Class *cls    = nullptr;
  };

  // Per worker thread state. Holding the RuleSet::Ptr here, rather
  // than copying it per request, is what makes the bare Class
  // pointers in the cache safe: the ruleset a thread is using cannot
  // be freed while that thread still references it, and the thread
  // only swaps to a newer one at a generation change, which also
  // clears the cache.
  struct TLS
  {
    RuleSet::Ptr rs;
    u64          generation = 0;
    CacheEntry   cache[CACHE_SIZE];
    int          next = 0;

    int applied_ioprio = qos::UNSET;
    int applied_nice   = qos::UNSET;
  };

  thread_local TLS tls;

  u64
  _now_ns()
  {
    struct timespec ts;

    ::clock_gettime(CLOCK_MONOTONIC,&ts);

    return ((static_cast<u64>(ts.tv_sec) * NS_PER_SEC) +
            static_cast<u64>(ts.tv_nsec));
  }

  void
  _refresh_ruleset()
  {
    const u64 gen = g_generation.load(std::memory_order_acquire);

    if(tls.generation == gen)
      return;

    {
      std::lock_guard<std::mutex> lk(g_mutex);
      tls.rs = ::_ruleset();
    }

    tls.generation = gen;
    for(auto &e : tls.cache)
      e = CacheEntry{};
    tls.next = 0;
  }

  const Class *
  _lookup(const u32 pid_,
          const u64 now_)
  {
    for(const auto &e : tls.cache)
      {
        if((e.pid == pid_) && (now_ < e.expire))
          return e.cls;
      }

    return nullptr;
  }

  void
  _remember(const u32    pid_,
            const u64    now_,
            const Class *cls_)
  {
    tls.cache[tls.next] = CacheEntry{pid_,now_ + CACHE_TTL_NS,cls_};
    tls.next = ((tls.next + 1) % CACHE_SIZE);
  }
}

void
qos::enable(const bool enable_)
{
  _enabled.store(enable_,std::memory_order_relaxed);
}

RuleSet::Ptr
qos::ruleset()
{
  std::lock_guard<std::mutex> lk(g_mutex);

  return ::_ruleset();
}

void
qos::set_ruleset(RuleSet::Ptr rs_)
{
  {
    std::lock_guard<std::mutex> lk(g_mutex);
    ::_ruleset() = std::move(rs_);
  }

  // Published after the swap so a thread that sees the new generation
  // is guaranteed to see the new ruleset.
  g_generation.fetch_add(1,std::memory_order_release);
}

int
qos::load_file(const std::string &path_,
               std::string       *err_)
{
  std::ifstream ifstrm;

  ifstrm.open(path_);
  if(!ifstrm.good())
    {
      *err_ = fmt::format("unable to open {}",path_);
      return -EIO;
    }

  std::stringstream ss;
  ss << ifstrm.rdbuf();

  RuleSet::Ptr rs = RuleSet::parse(ss.str(),err_);
  if(rs == nullptr)
    return -EINVAL;

  qos::set_ruleset(std::move(rs));

  {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_path = path_;
  }

  return 0;
}

void
qos::throttle(const Class *cls_,
              const u64    bytes_)
{
  if(cls_ == nullptr)
    return;

  cls_->requests.fetch_add(1,std::memory_order_relaxed);
  cls_->bytes.fetch_add(bytes_,std::memory_order_relaxed);

  if(cls_->rate == 0)
    return;

  u64 sleep_ns;

  {
    std::lock_guard<std::mutex> lk(cls_->bucket_mutex);

    const u64 now = ::_now_ns();

    if(cls_->refilled == 0)
      cls_->refilled = now;

    if(now > cls_->refilled)
      {
        // 128 bit so a bucket that has been idle for a long time
        // cannot overflow the refill computation. The result is
        // clamped to burst regardless.
        const unsigned __int128 elapsed = (now - cls_->refilled);
        const unsigned __int128 added   =
          ((elapsed * static_cast<unsigned __int128>(cls_->rate)) / NS_PER_SEC);

        if(added >= cls_->burst)
          cls_->tokens = cls_->burst;
        else
          cls_->tokens = std::min<u64>(cls_->burst,
                                       cls_->tokens + static_cast<u64>(added));

        cls_->refilled = now;
      }

    if(cls_->tokens >= bytes_)
      {
        cls_->tokens -= bytes_;
        return;
      }

    const u64 deficit = (bytes_ - cls_->tokens);

    cls_->tokens = 0;

    // Carry the debt by pushing the refill point into the future
    // rather than forgiving it. Tokens accrue in real time, so a
    // request that sleeps off its own deficit would otherwise find
    // those same tokens waiting for it -- and hand them to the next
    // request as well, which lets a class run at twice its rate.
    //
    // Moving `refilled` past the debt spends that time in advance. It
    // also means a request allowed through unthrottled below still
    // owes for its bytes; the next request in the class pays.
    const u64 need_ns = static_cast<u64>((static_cast<unsigned __int128>(deficit) *
                                          NS_PER_SEC) / cls_->rate);

    cls_->refilled += need_ns;
    if(cls_->refilled > (now + MAX_DEBT_NS))
      cls_->refilled = (now + MAX_DEBT_NS);

    sleep_ns = ((cls_->refilled > now) ? (cls_->refilled - now) : 0);
  }

  const u64 cap = qos::max_sleep_ns.load(std::memory_order_relaxed);
  if(sleep_ns > cap)
    {
      sleep_ns = cap;
      cls_->passed.fetch_add(1,std::memory_order_relaxed);
    }

  if(sleep_ns == 0)
    return;

  const int sleepers = (g_sleepers.fetch_add(1,std::memory_order_relaxed) + 1);
  if(sleepers > qos::max_sleepers.load(std::memory_order_relaxed))
    {
      g_sleepers.fetch_sub(1,std::memory_order_relaxed);
      cls_->passed.fetch_add(1,std::memory_order_relaxed);
      return;
    }

  struct timespec ts;
  ts.tv_sec  = (sleep_ns / NS_PER_SEC);
  ts.tv_nsec = (sleep_ns % NS_PER_SEC);
  ::nanosleep(&ts,nullptr);

  g_sleepers.fetch_sub(1,std::memory_order_relaxed);

  cls_->throttled.fetch_add(1,std::memory_order_relaxed);
  cls_->throttled_ns.fetch_add(sleep_ns,std::memory_order_relaxed);
}

void
qos::Apply::_slow_apply(const fuse_req_ctx_t *ctx_)
{
  ::_refresh_ruleset();

  const RuleSet *rs = tls.rs.get();
  if((rs == nullptr) || rs->inert())
    return;

  const u64 now = ::_now_ns();

  const Class *cls = ::_lookup(ctx_->pid,now);
  if(cls == nullptr)
    {
      // Only the fields some rule actually tests are read, so a
      // ruleset matching on cgroup alone costs one file read per
      // cache miss rather than two.
      const std::string cgroup = (rs->needs_cgroup()
                                  ? procfs::get_cgroup(ctx_->pid)
                                  : std::string{});
      const std::string comm   = (rs->needs_comm()
                                  ? procfs::get_name(ctx_->pid)
                                  : std::string{});

      cls = rs->classify(cgroup,comm,ctx_->uid,ctx_->gid);
      ::_remember(ctx_->pid,now,cls);
    }

  if(cls == nullptr)
    return;

  _cls = cls;

  if((cls->ioprio != qos::UNSET) && (cls->ioprio != tls.applied_ioprio))
    {
      if(::ioprio::set(0,cls->ioprio) >= 0)
        tls.applied_ioprio = cls->ioprio;
    }

  if((cls->nice != qos::UNSET) && (cls->nice != tls.applied_nice))
    {
      errno = 0;
      if(::setpriority(PRIO_PROCESS,0,cls->nice) == 0 || errno == 0)
        tls.applied_nice = cls->nice;
    }
}

std::string
qos::rules_path()
{
  std::lock_guard<std::mutex> lk(g_mutex);

  return g_path;
}

std::string
qos::stats()
{
  RuleSet::Ptr rs = qos::ruleset();
  std::string  out;

  if(rs == nullptr)
    return {};

  out += fmt::format("enabled={} sleepers-in-flight={} max-sleepers={} "
                     "max-sleep-ms={}\n",
                     (qos::enabled() ? "true" : "false"),
                     g_sleepers.load(std::memory_order_relaxed),
                     qos::max_sleepers.load(std::memory_order_relaxed),
                     (qos::max_sleep_ns.load(std::memory_order_relaxed) /
                      (1000 * 1000)));

  for(const auto &c : rs->classes())
    {
      out += fmt::format("{}: requests={} bytes={} throttled={} "
                         "throttled-ms={} passed={}\n",
                         c->name,
                         c->requests.load(std::memory_order_relaxed),
                         c->bytes.load(std::memory_order_relaxed),
                         c->throttled.load(std::memory_order_relaxed),
                         (c->throttled_ns.load(std::memory_order_relaxed) /
                          (1000 * 1000)),
                         c->passed.load(std::memory_order_relaxed));
    }

  return out;
}

void
qos::reset_stats()
{
  RuleSet::Ptr rs = qos::ruleset();

  if(rs == nullptr)
    return;

  for(const auto &c : rs->classes())
    {
      c->requests.store(0,std::memory_order_relaxed);
      c->bytes.store(0,std::memory_order_relaxed);
      c->throttled.store(0,std::memory_order_relaxed);
      c->throttled_ns.store(0,std::memory_order_relaxed);
      c->passed.store(0,std::memory_order_relaxed);
    }
}

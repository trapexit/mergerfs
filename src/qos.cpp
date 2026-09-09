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

#include "config.hpp"
#include "ioprio.hpp"
#include "procfs.hpp"

#include "fmt/core.h"

#include <errno.h>
#include <sys/resource.h>
#include <time.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

using qos::Class;
using qos::Direction;
using qos::RuleSet;
using qos::Subject;

namespace qos
{
  std::atomic<bool> _enabled{false};

  // Deliberately small. Each sleeping process thread is one fewer
  // thread draining the shared request queue, so the ceiling has to
  // stay well under process-thread-count. See qos.hpp.
  // -1 means "size it from the process thread pool". A fixed small
  // number under-enforces badly on a fast pool, and a large one risks
  // the head-of-line stall described in qos.hpp; half the pool keeps
  // the other half free to serve whoever is being protected.
  std::atomic<int> max_sleepers{-1};
  std::atomic<u64> max_sleep_ns{50ULL * 1000 * 1000};

  std::atomic<u64>    distress_floor_ns{50ULL * 1000 * 1000};
  std::atomic<double> distress_factor{3.0};
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

  // How recently a protected class must have issued I/O on a resource
  // for that resource to count as contended. Past this, yielding
  // classes go back to full speed.
  constexpr u64 CONTENTION_WINDOW_NS = 2 * NS_PER_SEC;

  // A protected request is "in distress" when the smoothed service
  // time exceeds this multiple of the quietest time the same resource
  // has managed -- but never below the absolute floor, so ordinary
  // jitter on a fast device does not trip the loop.
  // Multiplicative increase on distress, additive decrease per second
  // of calm. Deliberately asymmetric: a stutter has already been heard
  // by the time it is measured, so back off hard and return gently.
  constexpr double PRESSURE_UP_FACTOR = 1.5;
  constexpr double PRESSURE_UP_STEP   = 0.10;
  constexpr double PRESSURE_DOWN_PER_SEC = 0.20;

  std::mutex g_gov_mutex;

  // Governor state is keyed by resource and deliberately outlives
  // ruleset reloads: what a disk is doing does not change because
  // somebody edited a rules file.
  std::unordered_map<std::string,std::unique_ptr<qos::Governor>> g_governors;

  qos::Governor *
  _governor_for(const std::string &resource_)
  {
    std::lock_guard<std::mutex> lk(g_gov_mutex);

    auto i = g_governors.find(resource_);
    if(i != g_governors.end())
      return i->second.get();

    auto [it,inserted] = g_governors.emplace(resource_,
                                             std::make_unique<qos::Governor>());

    return it->second.get();
  }

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
    u64          key    = 0;   // path hash folded with the direction
    u64          expire = 0;
    const Class *cls    = nullptr;
    bool         valid  = false;
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

  // Resolves the -1 "auto" sentinel against the running thread pool.
  int
  _sleeper_limit()
  {
    const int configured = qos::max_sleepers.load(std::memory_order_relaxed);

    if(configured >= 0)
      return configured;

    // Resolved once: the thread count is fixed when the mount comes
    // up, and this sits on the throttle path where building a string
    // per request would be absurd.
    static const int half =
      []()
      {
        // The config holds the *requested* value, where <= 0 means
        // "decide for me" -- reading it raw yields 0 and a ceiling of
        // one sleeper. When it has not been pinned, mirror libfuse's
        // own default so the ceiling tracks the pool that actually
        // exists.
        int threads = std::atoi(cfg.process_thread_count.to_string().c_str());

        if(threads <= 0)
          {
            const int nproc = static_cast<int>(std::thread::hardware_concurrency());

            threads = std::min(8,std::max(2,nproc - 2));
          }

        return std::max(1,threads / 2);
      }();

    return half;
  }

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

  // The cache is keyed by the whole subject, not just the pid: with
  // path rules in play the same process reading two different folders
  // can land in two different classes.
  bool
  _lookup(const u32     pid_,
          const u64     key_,
          const u64     now_,
          const Class **cls_)
  {
    for(const auto &e : tls.cache)
      {
        if(e.valid && (e.pid == pid_) && (e.key == key_) && (now_ < e.expire))
          {
            *cls_ = e.cls;
            return true;
          }
      }

    return false;
  }

  void
  _remember(const u32    pid_,
            const u64    key_,
            const u64    now_,
            const Class *cls_)
  {
    tls.cache[tls.next] = CacheEntry{pid_,key_,now_ + CACHE_TTL_NS,cls_,true};
    tls.next = ((tls.next + 1) % CACHE_SIZE);
  }

  u64
  _subject_key(const std::string    *path_,
               const qos::Direction  dir_)
  {
    u64 key = ((dir_ == qos::Direction::WRITE) ? 1u : 0u);

    if(path_ != nullptr)
      key ^= (std::hash<std::string>{}(*path_) << 1);

    return key;
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

u64
qos::timing_start(const Apply &apply_)
{
  const Class *cls = apply_.cls();

  // Only protected classes drive the loop, so everything else pays
  // nothing -- not even a clock read.
  if((cls == nullptr) || !cls->protect)
    return 0;

  return ::_now_ns();
}

void
qos::timing_end(const Apply       &apply_,
                const std::string &resource_,
                const u64          started_)
{
  if(started_ == 0)
    return;

  const u64 now = ::_now_ns();
  const u64 latency = ((now > started_) ? (now - started_) : 0);

  qos::Governor *g = ::_governor_for(resource_);

  std::lock_guard<std::mutex> lk(g->mutex);

  g->protected_at = now;

  // Backing off only helps if there is something to back off. A read
  // that is slow on an otherwise idle disk is just a slow disk, and
  // throttling an absent competitor would not speed it up.
  const bool contended = (g->yielding_at &&
                          ((now - g->yielding_at) <= CONTENTION_WINDOW_NS));

  if(g->latency_ewma == 0)
    g->latency_ewma = latency;
  else
    g->latency_ewma = (((g->latency_ewma * 7) + latency) / 8);

  // The baseline is what this resource does when nothing is fighting
  // it. It follows a new low immediately but drifts back up only
  // slowly, so one unusually fast (cached) read cannot permanently
  // convince the governor that every later read is in distress.
  if(g->latency_base == 0)
    g->latency_base = g->latency_ewma;
  else if(g->latency_ewma < g->latency_base)
    g->latency_base = g->latency_ewma;
  else
    g->latency_base += ((g->latency_ewma - g->latency_base) / 1024);

  const u64 threshold =
    std::max<u64>(static_cast<u64>(g->latency_base *
                                   qos::distress_factor.load(std::memory_order_relaxed)),
                  qos::distress_floor_ns.load(std::memory_order_relaxed));

  const u64 elapsed = ((g->updated_at && (now > g->updated_at))
                       ? (now - g->updated_at)
                       : 0);

  if(contended && (g->latency_ewma > threshold))
    {
      g->pressure = std::min(1.0,
                             (g->pressure * PRESSURE_UP_FACTOR) + PRESSURE_UP_STEP);
      g->distress_events++;
    }
  else
    {
      const double secs = (static_cast<double>(elapsed) / NS_PER_SEC);
      g->pressure = std::max(0.0,
                             g->pressure - (PRESSURE_DOWN_PER_SEC * secs));
    }

  g->updated_at = now;
}

void
qos::note_yielding(const std::string &resource_)
{
  qos::Governor *g = ::_governor_for(resource_);

  const u64 now = ::_now_ns();

  std::lock_guard<std::mutex> lk(g->mutex);

  g->yielding_at = now;
}

double
qos::pressure(const std::string &resource_)
{
  qos::Governor *g = ::_governor_for(resource_);

  const u64 now = ::_now_ns();

  std::lock_guard<std::mutex> lk(g->mutex);

  // Nobody has been streaming off this resource lately, so there is
  // nothing to protect and nothing to yield to.
  if((g->protected_at == 0) ||
     ((now - g->protected_at) > CONTENTION_WINDOW_NS))
    {
      g->pressure = 0.0;
      return 0.0;
    }

  // Decay for time passed since the last protected sample so pressure
  // keeps falling even while the only traffic is the bulk class
  // reading this value.
  if(g->updated_at && (now > g->updated_at))
    {
      const double secs = (static_cast<double>(now - g->updated_at) / NS_PER_SEC);
      const double decayed = (g->pressure - (PRESSURE_DOWN_PER_SEC * secs));

      if(decayed < g->pressure)
        {
          g->pressure    = std::max(0.0,decayed);
          g->updated_at  = now;
        }
    }

  return g->pressure;
}

void
qos::throttle(const Apply       &apply_,
              const u64          bytes_,
              const std::string &resource_)
{
  const Class *cls = apply_.cls();

  if(cls == nullptr)
    return;

  cls->requests.fetch_add(1,std::memory_order_relaxed);
  cls->bytes.fetch_add(bytes_,std::memory_order_relaxed);

  // Protected traffic and critical dependencies are never delayed,
  // whatever else the ruleset says about them. Checked before any
  // rate is even resolved so a `rate=` accidentally left on such a
  // class cannot slow the stream waiting behind it.
  if(cls->immune())
    return;

  // An adaptive class normally carries no static rate at all -- its
  // whole allowance comes from the governor -- so it must not be
  // dismissed here for looking unlimited.
  if(!cls->limited() && !cls->adaptive())
    return;

  const RuleSet *rs = apply_.ruleset();
  if(rs == nullptr)
    return;

  // A percentage only becomes a number once the serving branch is
  // known, so the rate is resolved per request rather than at parse
  // time.
  u64 rate = rs->rate_for(cls,resource_);

  if(cls->adaptive())
    {
      // Tell the governor there is traffic here that *could* give way.
      // Without this it cannot distinguish "playback is slow because
      // downloads are hammering the disk" from "playback is slow
      // because the disk is slow", and only the first is worth
      // reacting to.
      qos::note_yielding(resource_);

      const double p = qos::pressure(resource_);

      if(p <= 0.0)
        {
          // Nothing protected is reading this resource, so a yielding
          // class runs at whatever it was configured for -- which for
          // downloads is normally no limit at all.
          if(rate == 0)
            return;
        }
      else
        {
          // Scaling needs a number to scale. An otherwise unlimited
          // class is measured against the resource's capacity; with no
          // capacity declared there is nothing to compute against and
          // it stays unlimited.
              const u64 base = (rate ? rate : rs->capacity(resource_));

          if(base == 0)
            return;

          const double share = (1.0 - ((p * cls->yield) / 100.0));
          u64 scaled = static_cast<u64>(base * std::max(0.0,share));

          // Yielding must never mean stopping: a floored class keeps
          // making progress, just slowly.
          const u64 floor = rs->floor_for(cls,resource_);
          if(scaled < floor)
            scaled = floor;
          if(scaled == 0)
            scaled = 1;

          rate = scaled;
        }
    }

  if(rate == 0)
    return;

  const u64 burst = (cls->burst ? cls->burst : rate);

  qos::Bucket *bucket = cls->bucket_for(resource_);

  u64 sleep_ns;

  {
    std::lock_guard<std::mutex> lk(bucket->mutex);

    const u64 now = ::_now_ns();

    if(!bucket->primed)
      {
        bucket->tokens   = burst;
        bucket->refilled = now;
        bucket->primed   = true;
      }

    if(now > bucket->refilled)
      {
        // 128 bit so a bucket that has been idle for a long time
        // cannot overflow the refill computation. The result is
        // clamped to burst regardless.
        const unsigned __int128 elapsed = (now - bucket->refilled);
        const unsigned __int128 added   =
          ((elapsed * static_cast<unsigned __int128>(rate)) / NS_PER_SEC);

        if(added >= burst)
          bucket->tokens = burst;
        else
          bucket->tokens = std::min<u64>(burst,
                                         bucket->tokens + static_cast<u64>(added));

        bucket->refilled = now;
      }

    if(bucket->tokens >= bytes_)
      {
        bucket->tokens -= bytes_;
        return;
      }

    const u64 deficit = (bytes_ - bucket->tokens);

    bucket->tokens = 0;

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
                                          NS_PER_SEC) / rate);

    bucket->refilled += need_ns;
    if(bucket->refilled > (now + MAX_DEBT_NS))
      bucket->refilled = (now + MAX_DEBT_NS);

    sleep_ns = ((bucket->refilled > now) ? (bucket->refilled - now) : 0);
  }

  const u64 cap = qos::max_sleep_ns.load(std::memory_order_relaxed);
  if(sleep_ns > cap)
    {
      sleep_ns = cap;
      cls->passed.fetch_add(1,std::memory_order_relaxed);
    }

  if(sleep_ns == 0)
    return;

  const int sleepers = (g_sleepers.fetch_add(1,std::memory_order_relaxed) + 1);
  if(sleepers > ::_sleeper_limit())
    {
      g_sleepers.fetch_sub(1,std::memory_order_relaxed);
      cls->passed.fetch_add(1,std::memory_order_relaxed);
      return;
    }

  struct timespec ts;
  ts.tv_sec  = (sleep_ns / NS_PER_SEC);
  ts.tv_nsec = (sleep_ns % NS_PER_SEC);
  ::nanosleep(&ts,nullptr);

  g_sleepers.fetch_sub(1,std::memory_order_relaxed);

  cls->throttled.fetch_add(1,std::memory_order_relaxed);
  cls->throttled_ns.fetch_add(sleep_ns,std::memory_order_relaxed);
}

void
qos::Apply::_slow_apply(const fuse_req_ctx_t *ctx_,
                        const std::string    *fusepath_,
                        const Direction       dir_)
{
  ::_refresh_ruleset();

  const RuleSet *rs = tls.rs.get();
  if((rs == nullptr) || rs->inert())
    return;

  _rs = rs;

  const u64 now = ::_now_ns();
  const u64 key = ::_subject_key((rs->needs_path() ? fusepath_ : nullptr),dir_);

  const Class *cls;
  if(!::_lookup(ctx_->pid,key,now,&cls))
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
      const std::string cmdline = (rs->needs_cmdline()
                                   ? procfs::get_cmdline(ctx_->pid)
                                   : std::string{});

      Subject subject;
      subject.cgroup  = &cgroup;
      subject.comm    = &comm;
      subject.cmdline = &cmdline;
      subject.path   = fusepath_;
      subject.uid    = ctx_->uid;
      subject.gid    = ctx_->gid;
      subject.dir    = dir_;

      cls = rs->classify(subject);

      ::_remember(ctx_->pid,key,now,cls);
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
      if((::setpriority(PRIO_PROCESS,0,cls->nice) == 0) || (errno == 0))
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
                     ::_sleeper_limit(),
                     (qos::max_sleep_ns.load(std::memory_order_relaxed) /
                      (1000 * 1000)));

  {
    std::lock_guard<std::mutex> lk(g_gov_mutex);
    const u64 now = ::_now_ns();

    for(const auto &[resource,g] : g_governors)
      {
        std::lock_guard<std::mutex> glk(g->mutex);

        const bool contended = (g->protected_at &&
                                ((now - g->protected_at) <= CONTENTION_WINDOW_NS));

        out += fmt::format("resource {}: contended={} pressure={:.2f} "
                           "latency-ms={:.1f} baseline-ms={:.1f} distress={}\n",
                           resource,
                           (contended ? "yes" : "no"),
                           (contended ? g->pressure : 0.0),
                           (static_cast<double>(g->latency_ewma) / 1000000.0),
                           (static_cast<double>(g->latency_base) / 1000000.0),
                           g->distress_events);
      }
  }

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

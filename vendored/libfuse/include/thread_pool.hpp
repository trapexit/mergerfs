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

#include "bounded_queue.hpp"
#include "invocable.h"

#include "mutex.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <pthread.h>
#include <syslog.h>
#include <time.h>


struct ThreadPoolTraits : public moodycamel::ConcurrentQueueDefaultTraits
{
  static const int MAX_SEMA_SPINS = 1;
};


class ThreadPool
{
public:
  using PToken = moodycamel::ProducerToken;
  using CToken = moodycamel::ConsumerToken;

private:
  using Func  = ofats::any_invocable<void()>;
  using Queue = BoundedQueue<Func,ThreadPoolTraits>;

public:
  struct ScalingConfig
  {
    std::size_t  min_threads           = 2;
    std::size_t  max_threads           = 64;
    std::int64_t sample_interval_usecs = 100000;   // 100ms hill-climb sample period
    std::int64_t cooldown_usecs        = 200000;   // 200ms min time between scaling events
    std::int64_t idle_threshold_usecs  = 5000000;  // 5s worker idle timeout before self-exit
  };

  explicit
  ThreadPool(const unsigned     thread_count_      = std::thread::hardware_concurrency(),
             const unsigned     max_queue_depth_    = std::thread::hardware_concurrency() * 2,
             const std::string &name_               = {});
  explicit
  ThreadPool(const unsigned     thread_count_,
             const unsigned     max_queue_depth_,
             const std::string &name_,
             ScalingConfig      scaling_config_);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  enum WorkerAction { CONTINUE, BREAK, EXIT };

private:
  static void *start_routine(void *arg_);
  static void *start_routine_autoscale(void *arg_);
  static void *monitor_routine(void *arg_);

  WorkerAction _process_func(Func &func_);
  int  _add_thread_scaling_locked(const std::string &name_);
  int  _remove_thread_scaling_locked();
  void _remove_self(pthread_t t_);
  bool _try_remove_self_if_above(pthread_t t_, std::size_t min_count_);
  bool _try_claim_removal();
  void _maybe_grow_on_pressure();
  static std::uint64_t _now_usecs();
  static void _set_thread_name(pthread_t t_, const std::string &name_);

  std::atomic<std::size_t> _remove_count{0};

public:
  int add_thread(const std::string &name = {});
  int remove_thread();
  int set_threads(const std::size_t count);

public:
  template<typename FuncType>
  void
  enqueue_work(ThreadPool::PToken  &ptok_,
               FuncType           &&func_);

  template<typename FuncType>
  void
  enqueue_work(FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work(ThreadPool::PToken &ptok_,
                   FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work(FuncType &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work_for(ThreadPool::PToken  &ptok_,
                       std::int64_t         timeout_usecs_,
                       FuncType           &&func_);

  template<typename FuncType>
  bool
  try_enqueue_work_for(std::int64_t  timeout_usecs_,
                       FuncType    &&func_);

  template<typename FuncType>
  [[nodiscard]]
  std::future<std::invoke_result_t<FuncType>>
  enqueue_task(FuncType&& func_);

  // Auto-scaling enqueue: grows pool on queue pressure
  template<typename FuncType>
  void
  enqueue_work_autoscale(ThreadPool::PToken  &ptok_,
                         FuncType           &&func_);

  template<typename FuncType>
  void
  enqueue_work_autoscale(FuncType &&func_);

public:
  std::vector<pthread_t> threads() const;
  ThreadPool::PToken ptoken();

  // Introspection
  std::size_t thread_count() const;
  std::size_t queue_depth() const;
  std::size_t queue_capacity() const;
  std::uint64_t tasks_enqueued() const;
  std::uint64_t tasks_completed() const;
  std::uint64_t tasks_pending() const;

private:
  Queue _queue;

private:
  std::string const      _name;
  std::vector<pthread_t> _threads;
  mutable Mutex          _threads_mutex;
  Mutex                  _scaling_mutex;
  std::atomic<bool>      _stop{false};

  // Monitoring counters
  std::atomic<std::uint64_t> _tasks_enqueued{0};
  std::atomic<std::uint64_t> _tasks_completed{0};

  // Scaling state
  // _dynamic_scaling_enabled is derived from min/max bounds and is
  // immutable after construction.  _scaling_config is fully immutable
  // after construction.  _monitor_started tracks whether the monitor
  // thread exists so the destructor knows whether it must wake/join it.
  bool                    _dynamic_scaling_enabled;
  ScalingConfig const     _scaling_config;
  pthread_t         _monitor_thread;
  bool              _monitor_started;
  // Raw pthreads (not Mutex/LockGuard) because the condvar requires
  // CLOCK_MONOTONIC via pthread_condattr_setclock, which the Mutex
  // wrapper doesn't support.
  pthread_mutex_t   _monitor_mutex;
  pthread_cond_t    _monitor_cond;

  std::atomic<std::uint64_t> _last_scale_time_usecs{0};
};


inline
std::uint64_t
ThreadPool::_now_usecs()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC,&ts);
  return (std::uint64_t)ts.tv_sec * 1000000ULL + (std::uint64_t)ts.tv_nsec / 1000ULL;
}


inline
void
ThreadPool::_set_thread_name(pthread_t            t_,
                             const std::string   &name_)
{
  if(name_.empty())
    return;
  std::string n = name_.substr(0,15);
  pthread_setname_np(t_,n.c_str());
}


inline
ThreadPool::ThreadPool(const unsigned        thread_count_,
                       const unsigned        max_queue_depth_,
                       const std::string    &name_)
  : ThreadPool(thread_count_,
               max_queue_depth_,
               name_,
               ScalingConfig{thread_count_, thread_count_})
{
}


inline
ThreadPool::ThreadPool(const unsigned        thread_count_,
                       const unsigned        max_queue_depth_,
                       const std::string    &name_,
                       ScalingConfig         scaling_config_)
  : _queue(max_queue_depth_ > 0
           ? max_queue_depth_
           : throw std::invalid_argument("threadpool: max_queue_depth must be > 0")),
    _name(name_),
    _dynamic_scaling_enabled(scaling_config_.min_threads < scaling_config_.max_threads),
    _scaling_config(std::move(scaling_config_)),
    _monitor_thread(),
    _monitor_started(false),
    _monitor_mutex(PTHREAD_MUTEX_INITIALIZER),
    _monitor_cond()
{
  // Use CLOCK_MONOTONIC for the monitor condvar so timed waits
  // are immune to wall-clock adjustments (NTP, settimeofday).
  {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
    pthread_cond_init(&_monitor_cond,&attr);
    pthread_condattr_destroy(&attr);
  }

  // Clamp initial thread count to configured bounds.
  std::size_t count = thread_count_;
  if(count < _scaling_config.min_threads)
    count = _scaling_config.min_threads;
  if(count > _scaling_config.max_threads)
    count = _scaling_config.max_threads;

  sigset_t oldset;
  sigset_t newset;

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);

  _threads.reserve(count);
  for(std::size_t i = 0; i < count; ++i)
    {
      int rv;
      pthread_t t;
      auto routine = _dynamic_scaling_enabled
        ? ThreadPool::start_routine_autoscale
        : ThreadPool::start_routine;

      rv = pthread_create(&t,NULL,routine,this);
      if(rv != 0)
        {
          syslog(LOG_WARNING,
                 "threadpool (%s): error spawning thread - %d (%s)",
                 _name.c_str(),
                 rv,
                 strerror(rv));
          continue;
        }

      _set_thread_name(t,_name);
      _threads.push_back(t);
    }

  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(_threads.empty())
    {
      pthread_cond_destroy(&_monitor_cond);
      pthread_mutex_destroy(&_monitor_mutex);
      throw std::runtime_error("threadpool: failed to spawn any threads");
    }

  syslog(LOG_DEBUG,
         "threadpool (%s): spawned %zu threads w/ max queue depth %u",
         _name.c_str(),
         _threads.size(),
         max_queue_depth_);

  // Start the monitor only when dynamic scaling can actually change
  // the thread count. Fixed-size pools still use _scaling_config for
  // their initial count, but don't need the monitor thread.
  if(_dynamic_scaling_enabled)
    {
      sigfillset(&newset);
      pthread_sigmask(SIG_BLOCK,&newset,&oldset);

      int rv = pthread_create(&_monitor_thread,NULL,
                              ThreadPool::monitor_routine,this);
      pthread_sigmask(SIG_SETMASK,&oldset,NULL);

      if(rv != 0)
        {
          syslog(LOG_WARNING,
                 "threadpool (%s): failed to spawn monitor thread - %d (%s); "
                 "continuing without hill-climb monitor",
                 _name.c_str(),
                 rv,
                 strerror(rv));
        }
      else
        {
          _monitor_started = true;
          {
            std::string mon_name = _name.empty() ? "tp.monitor" : (_name + ".mon");
            _set_thread_name(_monitor_thread,mon_name);
          }
          syslog(LOG_DEBUG,
                 "threadpool (%s): auto-scaling enabled [%zu, %zu]",
                 _name.c_str(),
                 _scaling_config.min_threads,
                 _scaling_config.max_threads);
        }
    }
}


inline
ThreadPool::~ThreadPool()
{
  _stop.store(true,std::memory_order_release);

  // Wake the monitor thread so it exits immediately instead of
  // sleeping for up to one sample interval.
  if(_monitor_started)
    {
      pthread_mutex_lock(&_monitor_mutex);
      pthread_cond_signal(&_monitor_cond);
      pthread_mutex_unlock(&_monitor_mutex);
      pthread_join(_monitor_thread,NULL);
    }

  // Snapshot threads under lock to close the race with _remove_self.
  // After _stop is set, _remove_self becomes a no-op, so no thread
  // will detach or erase itself from this point forward.
  std::vector<pthread_t> snapshot;
  {
    mutex_lockguard(_threads_mutex);
    snapshot = _threads;
  }

  syslog(LOG_DEBUG,
         "threadpool (%s): destroying %zu threads",
         _name.c_str(),
         snapshot.size());

  // Enqueue one poison pill per thread
  for(std::size_t i = 0; i < snapshot.size(); ++i)
    _queue.enqueue_unbounded(Func{});

  for(auto t : snapshot)
    pthread_join(t,NULL);

  // Destroy the monitor condvar/mutex only after all workers are
  // joined, so no thread can still reference them.
  pthread_cond_destroy(&_monitor_cond);
  pthread_mutex_destroy(&_monitor_mutex);
}


// Shared post-dequeue logic for both worker loops.
// Returns BREAK for poison pill, EXIT for _remove_count claim, CONTINUE otherwise.
inline
ThreadPool::WorkerAction
ThreadPool::_process_func(Func &func_)
{
  // Empty func is a poison pill — either a shutdown signal from
  // the destructor or a wakeup from _remove_thread_scaling_locked.
  if(!func_)
    {
      // During shutdown the destructor owns all joins; exit without detach.
      if(_stop.load(std::memory_order_acquire))
        return BREAK;

      // Try to claim a pending shrink request.  Each call to
      // _remove_thread_scaling_locked increments _remove_count and
      // enqueues a wakeup pill.  If the count is still positive,
      // this pill is ours — detach and exit.  If the count is zero,
      // a worker already claimed the decrement via the task-
      // completion path below; this pill is stale — keep working.
      if(_try_claim_removal())
        {
          _remove_self(pthread_self());
          return EXIT;
        }

      return CONTINUE;
    }

  try
    {
      func_();
    }
  catch(const std::exception &e)
    {
      syslog(LOG_CRIT,
             "threadpool (%s): uncaught exception caught by worker - %s",
             _name.c_str(),
             e.what());
    }
  catch(...)
    {
      syslog(LOG_CRIT,
             "threadpool (%s): uncaught non-standard exception caught by worker",
             _name.c_str());
    }

  // Force destruction to release resources
  func_ = {};

  _tasks_completed.fetch_add(1,std::memory_order_relaxed);

  // _remove_thread_scaling_locked increments _remove_count.
  // Workers race to claim a decrement after each task; the winner
  // detaches itself and exits.  During shutdown (_stop set),
  // _remove_self is a no-op, so the destructor still joins us.
  if(_try_claim_removal())
    {
      _remove_self(pthread_self());
      return EXIT;
    }

  return CONTINUE;
}


// Standard worker loop: blocks indefinitely on dequeue.
inline
void*
ThreadPool::start_routine(void *arg_)
{
  ThreadPool *btp = static_cast<ThreadPool*>(arg_);

  ThreadPool::Func func;
  ThreadPool::Queue &q = btp->_queue;
  ThreadPool::CToken ctok(btp->_queue.make_ctoken());

  while(true)
    {
      q.wait_dequeue(ctok,func);

      WorkerAction action = btp->_process_func(func);
      if(action != CONTINUE)
        break;
    }

  return nullptr;
}


// Auto-scaling worker loop: uses timed dequeue so idle threads can self-exit.
inline
void*
ThreadPool::start_routine_autoscale(void *arg_)
{
  ThreadPool *btp = static_cast<ThreadPool*>(arg_);

  ThreadPool::Func func;
  ThreadPool::Queue &q = btp->_queue;
  ThreadPool::CToken ctok(btp->_queue.make_ctoken());
  std::string const name = btp->_name;
  std::int64_t const idle_threshold = btp->_scaling_config.idle_threshold_usecs;

  while(true)
    {
      bool got = q.try_wait_dequeue_for(ctok,func,
                                        idle_threshold);

      if(!got)
        {
          // Timed out - check if we should shrink
          if(btp->_stop.load(std::memory_order_acquire))
            break;

          // Snapshot the minimum thread count BEFORE calling
          // _try_remove_self_if_above.  If that method returns true
          // the thread is detached and removed from _threads — the
          // destructor's snapshot won't include it, so ~ThreadPool
          // may run concurrently on another thread.  Reading
          // btp->_scaling_config as an argument to the call would
          // dereference the pool object before the lock inside
          // _try_remove_self_if_above serializes with the destructor.
          //
          // name was already copied into a local at function entry,
          // so the syslog below is also safe.
          // Floor at 1 to match _remove_thread_scaling_locked's
          // min_allowed floor.  Without this, min_threads == 0
          // would let all workers self-exit, draining the pool
          // to zero threads and stalling non-autoscale enqueues.
          std::size_t const min_thr = std::max(btp->_scaling_config.min_threads,
                                               (std::size_t)1);
          if(btp->_try_remove_self_if_above(pthread_self(),min_thr))
            {
              syslog(LOG_DEBUG,
                     "threadpool (%s): idle self-exit",
                     name.c_str());
              return nullptr;
            }

          continue;
        }

      WorkerAction action = btp->_process_func(func);
      if(action != CONTINUE)
        break;
    }

  return nullptr;
}


// Hill-climbing monitor thread.
// Periodically samples throughput, perturbs thread count by +/-1,
// and moves in the direction that improves throughput.
// Uses an EMA to smooth throughput noise and requires two consecutive
// declining samples before reversing direction.
inline
void*
ThreadPool::monitor_routine(void *arg_)
{
  ThreadPool *btp = static_cast<ThreadPool*>(arg_);

  const std::int64_t interval = btp->_scaling_config.sample_interval_usecs;
  int direction = 1;
  int decline_streak = 0;
  std::uint64_t prev_completed = btp->_tasks_completed.load(std::memory_order_relaxed);
  double ema_throughput = 0.0;
  constexpr double ema_alpha = 0.3;  // weight for new samples
  constexpr int decline_threshold = 2;
  int warmup_samples = 0;

  // Track expected thread count after the last perturbation so we
  // can detect external scaling (fast-path grow, idle self-exit)
  // and avoid misattributing their throughput effects to our action.
  std::size_t expected_count = btp->thread_count();
  bool acted_last_cycle = false;

  while(true)
    {
      // Sleep for one sample interval, but wake immediately if the
      // destructor signals _monitor_cond.
      std::uint64_t t0 = _now_usecs();
      {
        struct timespec abs_ts;
        clock_gettime(CLOCK_MONOTONIC,&abs_ts);
        abs_ts.tv_nsec += (interval % 1000000) * 1000;
        abs_ts.tv_sec  += interval / 1000000;
        if(abs_ts.tv_nsec >= 1000000000L)
          {
            abs_ts.tv_sec  += 1;
            abs_ts.tv_nsec -= 1000000000L;
          }

        // Check _stop under _monitor_mutex to close the lost-wakeup
        // race: without this, the destructor could set _stop and
        // signal between our check and the timedwait, losing the
        // signal and delaying shutdown by up to one sample interval.
        pthread_mutex_lock(&btp->_monitor_mutex);
        if(btp->_stop.load(std::memory_order_acquire))
          {
            pthread_mutex_unlock(&btp->_monitor_mutex);
            break;
          }
        pthread_cond_timedwait(&btp->_monitor_cond,
                               &btp->_monitor_mutex,
                               &abs_ts);
        pthread_mutex_unlock(&btp->_monitor_mutex);
      }

      if(btp->_stop.load(std::memory_order_acquire))
        break;

      // Skip spurious wakeups.  A short interval yields an
      // artificially low throughput delta that would cause the EMA
      // to decline and potentially trigger a false direction
      // reversal in the hill-climber.  Reset prev_completed so the
      // next real cycle doesn't accumulate the skipped interval's
      // completions into a single inflated throughput sample.
      std::uint64_t elapsed = _now_usecs() - t0;
      if(elapsed < (std::uint64_t)interval / 2)
        {
          prev_completed = btp->_tasks_completed.load(std::memory_order_relaxed);
          continue;
        }

      std::uint64_t completed = btp->_tasks_completed.load(std::memory_order_relaxed);
      std::uint64_t raw_throughput = completed - prev_completed;
      prev_completed = completed;

      // Warm up: seed the EMA with the first two samples before acting.
      if(warmup_samples < 2)
        {
          ema_throughput = (warmup_samples == 0)
            ? (double)raw_throughput
            : ema_alpha * (double)raw_throughput + (1.0 - ema_alpha) * ema_throughput;
          ++warmup_samples;
          continue;
        }

      double prev_ema = ema_throughput;
      ema_throughput = ema_alpha * (double)raw_throughput + (1.0 - ema_alpha) * ema_throughput;

      // Only adjust if there's meaningful work happening.
      // Reset direction to +1 (bias toward growth) so a stale
      // shrink direction from before the idle period doesn't
      // penalize the pool when work resumes.  Also reset warmup
      // so the EMA re-seeds from fresh samples rather than
      // comparing against a stale pre-idle value.
      if(ema_throughput < 0.5 && prev_ema < 0.5)
        {
          decline_streak = 0;
          direction = 1;
          warmup_samples = 0;
          prev_completed = completed;
          continue;
        }

      std::size_t count = btp->thread_count();

      // If external scaling changed the thread count since our last
      // perturbation, we can't attribute the throughput delta to our
      // action.  Reset the streak and skip this cycle.
      if(acted_last_cycle && count != expected_count)
        {
          decline_streak = 0;
          acted_last_cycle = false;
          expected_count = count;
          prev_completed = completed;
          continue;
        }
      acted_last_cycle = false;

      // Hill-climb: did smoothed throughput improve since last sample?
      // Strict > so flat throughput (saturated/oversized pool) counts
      // as no improvement, allowing the hill-climber to eventually
      // shrink an oversized pool rather than only relying on idle timeout.
      if(ema_throughput > prev_ema)
        {
          decline_streak = 0;
        }
      else
        {
          ++decline_streak;
          if(decline_streak >= decline_threshold)
            {
              direction = -direction;
              decline_streak = 0;
            }
          else
            {
              continue;  // wait for confirmation before reversing
            }
        }

      // Skip perturbation when already at a bound.  Without this,
      // at max_threads the grow is a no-op but the next reversal
      // succeeds at shrinking, creating a slow downward leak.
      // Symmetrically at min_threads a shrink no-op followed by a
      // grow reversal would needlessly inflate the pool.
      if(direction > 0 && count >= btp->_scaling_config.max_threads)
        continue;
      if(direction < 0 && count <= btp->_scaling_config.min_threads)
        continue;

      // Hold _scaling_mutex across the cooldown check and the scaling
      // action so the monitor and fast-path pressure grow cannot both
      // pass the cooldown gate simultaneously.
      {
        mutex_lockguard(btp->_scaling_mutex);

        std::uint64_t now  = _now_usecs();
        std::uint64_t last = btp->_last_scale_time_usecs.load(std::memory_order_relaxed);
        if((now - last) < (std::uint64_t)btp->_scaling_config.cooldown_usecs)
          continue;

        // Re-read thread count under _scaling_mutex so the bound
        // check is authoritative.  The count read at line 632 is a
        // pre-lock hint used only by the skip-at-bounds check above;
        // by the time we get here an idle self-exit or fast-path grow
        // may have changed it.
        count = btp->thread_count();

        if(direction > 0 && count < btp->_scaling_config.max_threads)
          {
            if(btp->_add_thread_scaling_locked({}) == 0)
              {
                btp->_last_scale_time_usecs.store(now,
                                                  std::memory_order_relaxed);
                acted_last_cycle = true;
                // Use count + 1 rather than thread_count() so the
                // expected value reflects the intended outcome of our
                // action, not the racy current state.  For grow this
                // is equivalent (push_back is synchronous) but for
                // consistency and to avoid an extra lock acquisition,
                // compute it arithmetically.
                expected_count = count + 1;
                syslog(LOG_DEBUG,
                       "threadpool (%s): hill-climb grow to %zu threads "
                       "(ema %.1f -> %.1f)",
                       btp->_name.c_str(),
                       expected_count,
                       prev_ema,
                       ema_throughput);
              }
          }
        else if(direction < 0 && count > btp->_scaling_config.min_threads)
          {
            if(btp->_remove_thread_scaling_locked() == 0)
              {
                btp->_last_scale_time_usecs.store(now,
                                                  std::memory_order_relaxed);
                acted_last_cycle = true;
                // _remove_thread_scaling_locked only increments
                // _remove_count and enqueues a wakeup pill — it does
                // not synchronously erase from _threads.  Using
                // thread_count() here would return the pre-shrink
                // count, causing the next cycle to misattribute the
                // deferred removal to external scaling interference.
                expected_count = count - 1;
                syslog(LOG_DEBUG,
                       "threadpool (%s): hill-climb shrink to %zu threads "
                       "(ema %.1f -> %.1f)",
                       btp->_name.c_str(),
                       expected_count,
                       prev_ema,
                       ema_throughput);
              }
          }
      }
    }

  return nullptr;
}


// Try to claim one pending removal.  Returns true if a _remove_count
// decrement was successfully claimed, false if the count was zero.
inline
bool
ThreadPool::_try_claim_removal()
{
  std::size_t count = _remove_count.load(std::memory_order_relaxed);
  while(count > 0)
    {
      if(_remove_count.compare_exchange_weak(count,count - 1,
                                              std::memory_order_acq_rel))
        return true;
    }
  return false;
}


inline
void
ThreadPool::_remove_self(pthread_t t_)
{
  mutex_lockguard(_threads_mutex);

  // Check _stop under the lock so the destructor can't snapshot this
  // thread for joining and then race with us detaching ourselves.
  if(_stop.load(std::memory_order_acquire))
    return;

  auto it = std::find(_threads.begin(),_threads.end(),t_);
  if(it != _threads.end())
    {
      _threads.erase(it);
      pthread_detach(t_);
    }
}


// Atomically check thread count and remove self if above min_count_.
// Returns true if removed, false if at or below threshold.
//
// Acquires _scaling_mutex then _threads_mutex (matching the lock
// ordering used by _maybe_grow_on_pressure and the monitor) so that
// idle self-exit is serialized with set_threads, _remove_thread, and
// _add_thread.  Without _scaling_mutex, an idle self-exit racing with
// set_threads could cause set_threads to overshoot because it reads
// _threads.size() under _threads_mutex but the idle exit shrinks the
// pool outside _scaling_mutex.
inline
bool
ThreadPool::_try_remove_self_if_above(pthread_t    t_,
                                      std::size_t  min_count_)
{
  mutex_lockguard(_scaling_mutex);
  mutex_lockguard(_threads_mutex);

  // Check _stop under the lock to close the TOCTOU race with the
  // destructor.  The destructor sets _stop, then acquires
  // _threads_mutex to snapshot _threads.  By checking _stop here
  // under the same lock, either:
  //   (a) we hold the lock first, see _stop==false, detach and
  //       erase ourselves — the destructor's snapshot won't
  //       include us, so it won't join a detached thread; or
  //   (b) the destructor sets _stop first — we see _stop==true
  //       and bail out, remaining in _threads for the destructor
  //       to join.
  if(_stop.load(std::memory_order_acquire))
    return false;

  // Account for pending removals that haven't been claimed yet.
  // Without this, an idle self-exit can race with a monitor shrink:
  // the monitor increments _remove_count (a pending removal), and
  // the idle thread also removes itself, pushing the pool below
  // min_count_.
  std::size_t pending = _remove_count.load(std::memory_order_relaxed);
  std::size_t effective = (_threads.size() > pending)
    ? (_threads.size() - pending)
    : 0;
  if(effective <= min_count_)
    return false;

  auto it = std::find(_threads.begin(),_threads.end(),t_);
  if(it == _threads.end())
    return false;

  _threads.erase(it);
  pthread_detach(t_);
  return true;
}


inline
void
ThreadPool::_maybe_grow_on_pressure()
{
  if(!_dynamic_scaling_enabled)
    return;

  // Don't spawn threads if the pool is shutting down.  Without this
  // check a caller could grow the pool after the destructor has
  // already sized its poison-pill batch, leaving the new thread
  // blocking forever on wait_dequeue with no pill to wake it.
  if(_stop.load(std::memory_order_acquire))
    return;

  // Quick check before contending on the lock.  Uses a stale read
  // but the authoritative re-check happens under the lock below.
  {
    std::uint64_t now  = _now_usecs();
    std::uint64_t last = _last_scale_time_usecs.load(std::memory_order_relaxed);
    if((now - last) < (std::uint64_t)_scaling_config.cooldown_usecs)
      return;
  }

  // Hold _scaling_mutex so the thread-count check and add are atomic
  // with respect to other scaling operations.
  mutex_lockguard(_scaling_mutex);

  std::uint64_t now  = _now_usecs();
  std::uint64_t last = _last_scale_time_usecs.load(std::memory_order_relaxed);
  if((now - last) < (std::uint64_t)_scaling_config.cooldown_usecs)
    return;

  if(_add_thread_scaling_locked({}) != 0)
    return;
  _last_scale_time_usecs.store(now,std::memory_order_relaxed);

  syslog(LOG_DEBUG,
         "threadpool (%s): fast-path grow to %zu threads (queue full)",
         _name.c_str(),
         thread_count());
}


inline
int
ThreadPool::_add_thread_scaling_locked(const std::string &name_)
{
  if(_dynamic_scaling_enabled)
    {
      mutex_lockguard(_threads_mutex);
      if(_threads.size() >= _scaling_config.max_threads)
        return -EAGAIN;
    }

  int rv;
  pthread_t t;
  sigset_t oldset;
  sigset_t newset;
  std::string name;

  name = (name_.empty() ? _name : name_);

  auto routine = _dynamic_scaling_enabled
    ? ThreadPool::start_routine_autoscale
    : ThreadPool::start_routine;

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);
  rv = pthread_create(&t,NULL,routine,this);
  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(rv != 0)
    {
      syslog(LOG_WARNING,
             "threadpool (%s): error spawning thread - %d (%s)",
             _name.c_str(),
             rv,
             strerror(rv));

      // Treat creation failure as a scaling event so the cooldown
      // timer suppresses immediate retries from the monitor and
      // fast-path pressure grow, avoiding syslog spam under
      // sustained resource pressure.
      if(_dynamic_scaling_enabled)
        _last_scale_time_usecs.store(_now_usecs(),std::memory_order_relaxed);

      return -rv;
    }

  _set_thread_name(t,name);

  {
    mutex_lockguard(_threads_mutex);
    _threads.push_back(t);
  }

  return 0;
}

inline
int
ThreadPool::add_thread(const std::string &name_)
{
  mutex_lockguard(_scaling_mutex);
  return _add_thread_scaling_locked(name_);
}


inline
int
ThreadPool::_remove_thread_scaling_locked()
{
  // Hold _threads_mutex across both the size check and the
  // _remove_count increment so that a concurrent removal (idle
  // self-exit or another _remove_thread call) cannot shrink the
  // pool below min_allowed between the check and the signal.
  {
    mutex_lockguard(_threads_mutex);
    std::size_t min_allowed = 1;
    if(_dynamic_scaling_enabled && _scaling_config.min_threads > min_allowed)
      min_allowed = _scaling_config.min_threads;
    std::size_t pending = _remove_count.load(std::memory_order_relaxed);
    std::size_t effective = (_threads.size() > pending)
      ? (_threads.size() - pending)
      : 0;
    if(effective <= min_allowed)
      return -EINVAL;

    // Increment the atomic counter while still holding the lock.
    // A worker will claim the decrement either:
    //   (a) in the poison-pill handler after dequeuing the wakeup, or
    //   (b) in the task-completion CAS loop in _process_func.
    // In case (b) the wakeup pill becomes stale and is harmlessly
    // ignored (the pill handler sees _remove_count == 0 and continues).
    _remove_count.fetch_add(1,std::memory_order_relaxed);
  }

  _queue.enqueue_unbounded(Func{});

  return 0;
}

inline
int
ThreadPool::remove_thread()
{
  mutex_lockguard(_scaling_mutex);
  return _remove_thread_scaling_locked();
}

inline
int
ThreadPool::set_threads(const std::size_t count_)
{
  mutex_lockguard(_scaling_mutex);

  if(count_ == 0)
    return -EINVAL;

  // Clamp to autoscale bounds so external callers can't push the
  // pool outside the range the monitor and idle-exit logic expect.
  std::size_t count = count_;
  if(_dynamic_scaling_enabled)
    {
      if(count < _scaling_config.min_threads)
        count = _scaling_config.min_threads;
      if(count > _scaling_config.max_threads)
        count = _scaling_config.max_threads;
    }

  std::size_t current;
  std::size_t pending;

  {
    mutex_lockguard(_threads_mutex);
    current = _threads.size();
    // Read _remove_count under the same lock to get a consistent
    // snapshot.  Without the lock a worker could claim a
    // _remove_count decrement and erase itself from _threads
    // between the two reads, causing set_threads to double-count
    // the shrink and over-remove.
    pending = _remove_count.load(std::memory_order_relaxed);
  }

  std::size_t effective = (current > pending) ? (current - pending) : 0;

  int rv = 0;

  for(std::size_t i = effective; i < count; ++i)
    {
      int err = _add_thread_scaling_locked({});
      if(err != 0)
        {
          rv = err;
          break;
        }
    }

  for(std::size_t i = count; i < effective; ++i)
    {
      int err = _remove_thread_scaling_locked();
      if(err != 0)
        {
          rv = err;
          break;
        }
    }

  return rv;
}

template<typename FuncType>
inline
void
ThreadPool::enqueue_work(ThreadPool::PToken  &ptok_,
                         FuncType           &&func_)
{
  _queue.enqueue(ptok_,
                 std::forward<FuncType>(func_));
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
}

template<typename FuncType>
inline
void
ThreadPool::enqueue_work(FuncType &&func_)
{
  _queue.enqueue(std::forward<FuncType>(func_));
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work(ThreadPool::PToken &ptok_,
                             FuncType &&func_)
{
  if(!_queue.try_enqueue(ptok_,std::forward<FuncType>(func_)))
    return false;
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
  return true;
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work(FuncType &&func_)
{
  if(!_queue.try_enqueue(std::forward<FuncType>(func_)))
    return false;
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
  return true;
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work_for(ThreadPool::PToken  &ptok_,
                                 std::int64_t         timeout_usecs_,
                                 FuncType           &&func_)
{
  if(!_queue.try_enqueue_for(ptok_,timeout_usecs_,std::forward<FuncType>(func_)))
    return false;
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
  return true;
}


template<typename FuncType>
inline
bool
ThreadPool::try_enqueue_work_for(std::int64_t  timeout_usecs_,
                                 FuncType    &&func_)
{
  if(!_queue.try_enqueue_for(timeout_usecs_,std::forward<FuncType>(func_)))
    return false;
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
  return true;
}


template<typename FuncType>
inline
void
ThreadPool::enqueue_work_autoscale(ThreadPool::PToken  &ptok_,
                                   FuncType           &&func_)
{
  // Materialize into a Func up front so we own the value across
  // the try/block-enqueue boundary without a double std::forward.
  //
  // INVARIANT: BoundedQueue::try_enqueue checks the semaphore
  // (tryWait) *before* forwarding the item into the inner queue.
  // On failure the item is never moved-from, so f remains valid.
  // The assert below guards against future BoundedQueue changes
  // that could violate this ordering.
  Func f(std::forward<FuncType>(func_));

  if(_queue.try_enqueue(ptok_,std::move(f)))
    {
      _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
      return;
    }

  // If try_enqueue moved the callable despite returning false, the
  // Func is empty and would act as a poison pill — re-enqueuing it
  // could cause a worker to exit.  Throw so the caller notices
  // immediately rather than silently corrupting the pool.
  if(!static_cast<bool>(f))
    throw std::logic_error("BoundedQueue::try_enqueue moved the callable despite returning false");

  _maybe_grow_on_pressure();

  // Block-enqueue (will succeed once a worker frees a slot)
  _queue.enqueue(ptok_,std::move(f));
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
}


template<typename FuncType>
inline
void
ThreadPool::enqueue_work_autoscale(FuncType &&func_)
{
  // See ptok_ overload for invariant and safety note.
  Func f(std::forward<FuncType>(func_));

  if(_queue.try_enqueue(std::move(f)))
    {
      _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
      return;
    }

  if(!static_cast<bool>(f))
    throw std::logic_error("BoundedQueue::try_enqueue moved the callable despite returning false");

  _maybe_grow_on_pressure();

  // Block-enqueue (will succeed once a worker frees a slot)
  _queue.enqueue(std::move(f));
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
}


template<typename FuncType>
[[nodiscard]]
inline
std::future<std::invoke_result_t<FuncType>>
ThreadPool::enqueue_task(FuncType&& func_)
{
  using TaskReturnType = std::invoke_result_t<FuncType>;
  using Promise        = std::promise<TaskReturnType>;

  Promise promise;
  auto    future = promise.get_future();

  auto work =
    [promise_ = std::move(promise),
     func_    = std::forward<FuncType>(func_)]() mutable
    {
      try
        {
          if constexpr (std::is_void_v<TaskReturnType>)
            {
              func_();
              promise_.set_value();
            }
          else
            {
              promise_.set_value(func_());
            }
        }
      catch(...)
        {
          promise_.set_exception(std::current_exception());
        }
    };

  Func f(std::move(work));

  if(_dynamic_scaling_enabled)
    {
      if(!_queue.try_enqueue(std::move(f)))
        {
          if(!static_cast<bool>(f))
            throw std::logic_error("BoundedQueue::try_enqueue moved the callable despite returning false");
          _maybe_grow_on_pressure();
          _queue.enqueue(std::move(f));
        }
    }
  else
    {
      _queue.enqueue(std::move(f));
    }
  _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);

  return future;
}


inline
std::vector<pthread_t>
ThreadPool::threads() const
{
  mutex_lockguard(_threads_mutex);

  return _threads;
}


inline
ThreadPool::PToken
ThreadPool::ptoken()
{
  return _queue.make_ptoken();
}


inline
std::size_t
ThreadPool::thread_count() const
{
  mutex_lockguard(_threads_mutex);
  return _threads.size();
}

inline
std::size_t
ThreadPool::queue_depth() const
{
  return _queue.size_approx();
}

inline
std::size_t
ThreadPool::queue_capacity() const
{
  return _queue.max_depth();
}

inline
std::uint64_t
ThreadPool::tasks_enqueued() const
{
  return _tasks_enqueued.load(std::memory_order_relaxed);
}

inline
std::uint64_t
ThreadPool::tasks_completed() const
{
  return _tasks_completed.load(std::memory_order_relaxed);
}

// Approximate: the two counters are loaded non-atomically.
// Loading enqueued first biases the result toward zero (a
// completion between the two loads increases c beyond the
// snapshot of e, shrinking the delta), which is safer for
// scaling heuristics than over-reporting pressure.
// Suitable for monitoring and heuristics, not for synchronization.
inline
std::uint64_t
ThreadPool::tasks_pending() const
{
  auto e = _tasks_enqueued.load(std::memory_order_relaxed);
  auto c = _tasks_completed.load(std::memory_order_relaxed);
  return (e > c) ? (e - c) : 0;
}

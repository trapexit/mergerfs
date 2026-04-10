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
             const std::string &name_               = {},
             const ScalingConfig *scaling_config_    = nullptr);
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
  void _maybe_grow_on_pressure();
  static std::uint64_t _now_usecs();

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

  // Auto-scaling state
  bool          _autoscale_enabled;
  ScalingConfig _scaling_config;
  pthread_t     _monitor_thread;

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
ThreadPool::ThreadPool(const unsigned        thread_count_,
                       const unsigned        max_queue_depth_,
                       const std::string    &name_,
                       const ScalingConfig  *scaling_config_)
  : _queue(max_queue_depth_),
    _name(name_),
    _autoscale_enabled(scaling_config_ != nullptr),
    _scaling_config(),
    _monitor_thread()
{
  if(scaling_config_)
    _scaling_config = *scaling_config_;

  // Clamp initial thread count to ScalingConfig bounds when auto-scaling.
  std::size_t count = thread_count_;
  if(_autoscale_enabled)
    {
      if(count < _scaling_config.min_threads)
        count = _scaling_config.min_threads;
      if(count > _scaling_config.max_threads)
        count = _scaling_config.max_threads;
    }

  sigset_t oldset;
  sigset_t newset;

  sigfillset(&newset);
  pthread_sigmask(SIG_BLOCK,&newset,&oldset);

  _threads.reserve(count);
  for(std::size_t i = 0; i < count; ++i)
    {
      int rv;
      pthread_t t;
      auto routine = _autoscale_enabled
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

      if(not _name.empty())
        {
          std::string name = _name.substr(0, 15);
          pthread_setname_np(t, name.c_str());
        }

      _threads.push_back(t);
    }

  pthread_sigmask(SIG_SETMASK,&oldset,NULL);

  if(_threads.empty())
    throw std::runtime_error("threadpool: failed to spawn any threads");

  syslog(LOG_DEBUG,
         "threadpool (%s): spawned %zu threads w/ max queue depth %u",
         _name.c_str(),
         _threads.size(),
         max_queue_depth_);

  // Start monitor thread if auto-scaling enabled
  if(_autoscale_enabled)
    {
      sigfillset(&newset);
      pthread_sigmask(SIG_BLOCK,&newset,&oldset);

      int rv = pthread_create(&_monitor_thread,NULL,
                              ThreadPool::monitor_routine,this);
      pthread_sigmask(SIG_SETMASK,&oldset,NULL);

      if(rv != 0)
        {
          syslog(LOG_WARNING,
                 "threadpool (%s): failed to spawn monitor thread - %d (%s)",
                 _name.c_str(),
                 rv,
                 strerror(rv));
          _autoscale_enabled = false;
        }
      else
        {
          pthread_setname_np(_monitor_thread, "tp.monitor");
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

  // Stop monitor thread first
  if(_autoscale_enabled)
    {
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
}


// Shared post-dequeue logic for both worker loops.
// Returns BREAK for poison pill, EXIT for _remove_count claim, CONTINUE otherwise.
inline
ThreadPool::WorkerAction
ThreadPool::_process_func(Func &func_)
{
  // Empty func is a poison pill - exit cleanly.
  // If _stop is set, destructor owns the join - don't self-remove.
  // Otherwise this is a set_threads/remove_thread shrink - detach.
  if(!func_)
    {
      if(!_stop.load(std::memory_order_acquire))
        _remove_self(pthread_self());
      return BREAK;
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
  {
    std::size_t count = _remove_count.load(std::memory_order_relaxed);
    while(count > 0)
      {
        if(_remove_count.compare_exchange_weak(count,count - 1,
                                               std::memory_order_relaxed))
          {
            _remove_self(pthread_self());
            return EXIT;
          }
      }
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

  while(true)
    {
      bool got = q.try_wait_dequeue_for(ctok,func,
                                        btp->_scaling_config.idle_threshold_usecs);

      if(!got)
        {
          // Timed out - check if we should shrink
          if(btp->_stop.load(std::memory_order_acquire))
            break;

          // Copy state needed for the log message BEFORE detaching,
          // because _try_remove_self_if_above detaches the thread and
          // the destructor won't wait for us — btp may be destroyed
          // by the time we reach the syslog call.
          //
          // Safety: after _try_remove_self_if_above returns true the
          // thread is detached and removed from _threads.  The
          // destructor's snapshot won't include it, so pthread_join
          // is never called.  From this point we touch only locals
          // (name is a std::string copy made at function entry), so
          // there is no use-after-free even if ~ThreadPool runs
          // concurrently on another thread.
          if(btp->_try_remove_self_if_above(pthread_self(),
                                            btp->_scaling_config.min_threads))
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

  const struct timespec sleep_ts = {
    static_cast<time_t>(interval / 1000000),
    static_cast<long>((interval % 1000000) * 1000)
  };

  while(!btp->_stop.load(std::memory_order_acquire))
    {
      {
        struct timespec remaining = sleep_ts;
        while(nanosleep(&remaining,&remaining) == -1 && errno == EINTR)
          {
            if(btp->_stop.load(std::memory_order_acquire))
              break;
          }
      }

      if(btp->_stop.load(std::memory_order_acquire))
        break;

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
      // penalize the pool when work resumes.
      if(ema_throughput < 0.5 && prev_ema < 0.5)
        {
          decline_streak = 0;
          direction = 1;
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
          continue;
        }
      acted_last_cycle = false;

      // Hill-climb: did smoothed throughput improve since last sample?
      if(ema_throughput >= prev_ema)
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

      // Respect the global cooldown so the monitor and fast-path
      // pressure grow don't step on each other.
      {
        std::uint64_t now  = _now_usecs();
        std::uint64_t last = btp->_last_scale_time_usecs.load(std::memory_order_relaxed);
        if((now - last) < (std::uint64_t)btp->_scaling_config.cooldown_usecs)
          continue;
      }

      if(direction > 0 && count < btp->_scaling_config.max_threads)
        {
          btp->add_thread();
          btp->_last_scale_time_usecs.store(_now_usecs(),
                                            std::memory_order_relaxed);
          acted_last_cycle = true;
          expected_count = btp->thread_count();
          syslog(LOG_DEBUG,
                 "threadpool (%s): hill-climb grow to %zu threads "
                 "(ema %.1f -> %.1f)",
                 btp->_name.c_str(),
                 expected_count,
                 prev_ema,
                 ema_throughput);
        }
      else if(direction < 0 && count > btp->_scaling_config.min_threads)
        {
          btp->remove_thread();
          btp->_last_scale_time_usecs.store(_now_usecs(),
                                            std::memory_order_relaxed);
          acted_last_cycle = true;
          expected_count = btp->thread_count();
          syslog(LOG_DEBUG,
                 "threadpool (%s): hill-climb shrink to %zu threads "
                 "(ema %.1f -> %.1f)",
                 btp->_name.c_str(),
                 expected_count,
                 prev_ema,
                 ema_throughput);
        }
    }

  return nullptr;
}


inline
void
ThreadPool::_remove_self(pthread_t t_)
{
  // During shutdown the destructor owns all joins; don't detach.
  if(_stop.load(std::memory_order_acquire))
    return;

  mutex_lockguard(_threads_mutex);

  auto it = std::find(_threads.begin(),_threads.end(),t_);
  if(it != _threads.end())
    {
      _threads.erase(it);
      pthread_detach(t_);
    }
}


// Atomically check thread count and remove self if above min_count_.
// Returns true if removed, false if at or below threshold.
inline
bool
ThreadPool::_try_remove_self_if_above(pthread_t    t_,
                                      std::size_t  min_count_)
{
  if(_stop.load(std::memory_order_acquire))
    return false;

  mutex_lockguard(_threads_mutex);

  if(_threads.size() <= min_count_)
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
  if(!_autoscale_enabled)
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

  std::size_t count;
  {
    mutex_lockguard(_threads_mutex);
    count = _threads.size();
  }

  if(count >= _scaling_config.max_threads)
    return;

  _last_scale_time_usecs.store(now,std::memory_order_relaxed);

  _add_thread_scaling_locked({});
  syslog(LOG_DEBUG,
         "threadpool (%s): fast-path grow to %zu threads (queue full)",
         _name.c_str(),
         count + 1);
}


inline
int
ThreadPool::_add_thread_scaling_locked(const std::string &name_)
{
  int rv;
  pthread_t t;
  sigset_t oldset;
  sigset_t newset;
  std::string name;

  name = (name_.empty() ? _name : name_);

  auto routine = _autoscale_enabled
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
      return -rv;
    }

  if(not name.empty())
    {
      std::string n = name.substr(0, 15);
      pthread_setname_np(t,n.c_str());
    }

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
  {
    mutex_lockguard(_threads_mutex);
    std::size_t min_allowed = 1;
    if(_autoscale_enabled && _scaling_config.min_threads > min_allowed)
      min_allowed = _scaling_config.min_threads;
    if(_threads.size() <= min_allowed)
      return -EINVAL;
  }

  // Increment the atomic counter.  The next worker to finish a task
  // will claim the decrement in _process_func, detach itself via
  // _remove_self, and exit.  No heap allocation, no blocking.
  _remove_count.fetch_add(1,std::memory_order_relaxed);

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
  if(_autoscale_enabled)
    {
      if(count < _scaling_config.min_threads)
        count = _scaling_config.min_threads;
      if(count > _scaling_config.max_threads)
        count = _scaling_config.max_threads;
    }

  std::size_t current;

  {
    mutex_lockguard(_threads_mutex);
    current = _threads.size();
  }

  int rv = 0;

  for(std::size_t i = current; i < count; ++i)
    {
      rv = _add_thread_scaling_locked({});
      if(rv != 0)
        return rv;
    }

  for(std::size_t i = count; i < current; ++i)
    {
      rv = _remove_thread_scaling_locked();
      if(rv != 0)
        return rv;
    }

  return 0;
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
  // Safe: BoundedQueue::try_enqueue checks the semaphore before
  // forwarding, so f is not moved-from on failure.
  Func f(std::forward<FuncType>(func_));

  if(_queue.try_enqueue(std::move(f)))
    {
      _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
      return;
    }

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
  // See ptok_ overload for safety note on try_enqueue + move.
  Func f(std::forward<FuncType>(func_));

  if(_queue.try_enqueue(std::move(f)))
    {
      _tasks_enqueued.fetch_add(1,std::memory_order_relaxed);
      return;
    }

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

  _queue.enqueue(std::move(work));
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

// Approximate: the two counters are loaded non-atomically with
// relaxed ordering, so the result may momentarily read zero (or
// a lower value) while tasks complete between the two loads.
// Suitable for monitoring and heuristics, not for synchronization.
inline
std::uint64_t
ThreadPool::tasks_pending() const
{
  auto e = _tasks_enqueued.load(std::memory_order_relaxed);
  auto c = _tasks_completed.load(std::memory_order_relaxed);
  return (e > c) ? (e - c) : 0;
}

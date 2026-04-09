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

#include "moodycamel/blockingconcurrentqueue.h"
#include "moodycamel/lightweightsemaphore.h"

#include <cstdint>


// Wraps an unbounded BlockingConcurrentQueue with a semaphore to enforce
// a maximum queue depth. The semaphore tracks available slots:
//   - Producers wait (decrement) before enqueuing, blocking when full.
//   - Consumers signal (increment) after dequeuing, freeing a slot.
// This co-locates the invariant so the pairing cannot drift apart.
template<typename T, typename Traits = moodycamel::ConcurrentQueueDefaultTraits>
class BoundedQueue
{
public:
  using PToken = moodycamel::ProducerToken;
  using CToken = moodycamel::ConsumerToken;

private:
  using Queue = moodycamel::BlockingConcurrentQueue<T,Traits>;

  Queue _queue;
  moodycamel::LightweightSemaphore _slots;
  std::size_t const _max_depth;

public:
  explicit
  BoundedQueue(std::size_t max_depth_)
    : _queue(),
      _slots(max_depth_),
      _max_depth(max_depth_)
  {
  }

  BoundedQueue(const BoundedQueue&) = delete;
  BoundedQueue& operator=(const BoundedQueue&) = delete;
  BoundedQueue(BoundedQueue&&) = delete;
  BoundedQueue& operator=(BoundedQueue&&) = delete;

  // -- Blocking enqueue (waits indefinitely for a slot) -------------------

  template<typename U>
  void
  enqueue(U &&item_)
  {
    _slots.wait();
    _queue.enqueue(std::forward<U>(item_));
  }

  template<typename U>
  void
  enqueue(PToken &ptok_, U &&item_)
  {
    _slots.wait();
    _queue.enqueue(ptok_, std::forward<U>(item_));
  }

  // -- Non-blocking enqueue (fails immediately if full) -------------------

  template<typename U>
  bool
  try_enqueue(U &&item_)
  {
    if(!_slots.tryWait())
      return false;
    _queue.enqueue(std::forward<U>(item_));
    return true;
  }

  template<typename U>
  bool
  try_enqueue(PToken &ptok_, U &&item_)
  {
    if(!_slots.tryWait())
      return false;
    _queue.enqueue(ptok_, std::forward<U>(item_));
    return true;
  }

  // -- Timed enqueue (waits up to timeout_usecs_ for a slot) --------------

  template<typename U>
  bool
  try_enqueue_for(std::int64_t timeout_usecs_, U &&item_)
  {
    if(!_slots.wait(timeout_usecs_))
      return false;
    _queue.enqueue(std::forward<U>(item_));
    return true;
  }

  template<typename U>
  bool
  try_enqueue_for(PToken &ptok_, std::int64_t timeout_usecs_, U &&item_)
  {
    if(!_slots.wait(timeout_usecs_))
      return false;
    _queue.enqueue(ptok_, std::forward<U>(item_));
    return true;
  }

  // -- Unbounded enqueue (bypasses backpressure) --------------------------
  // Used for internal control messages (e.g. thread exit signals) that
  // must not be blocked by a full queue.

  template<typename U>
  void
  enqueue_unbounded(U &&item_)
  {
    _queue.enqueue(std::forward<U>(item_));
  }

  // -- Blocking dequeue (signals a slot after consuming) ------------------

  void
  wait_dequeue(CToken &ctok_, T &item_)
  {
    _queue.wait_dequeue(ctok_, item_);
    _slots.signal();
  }

  // -- Timed dequeue (waits up to timeout, signals slot if successful) -----

  bool
  try_wait_dequeue_for(CToken &ctok_, T &item_, std::int64_t timeout_usecs_)
  {
    if(!_queue.wait_dequeue_timed(ctok_, item_, timeout_usecs_))
      return false;
    _slots.signal();
    return true;
  }

  // -- Introspection -------------------------------------------------------

  std::size_t
  max_depth() const
  {
    return _max_depth;
  }

  std::size_t
  size_approx() const
  {
    return _queue.size_approx();
  }

  // -- Token creation -----------------------------------------------------

  PToken
  make_ptoken()
  {
    return PToken(_queue);
  }

  CToken
  make_ctoken()
  {
    return CToken(_queue);
  }
};

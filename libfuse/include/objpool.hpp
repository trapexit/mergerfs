/*
  ISC License

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

#include "mutex.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <type_traits>

struct DefaultAllocator
{
  void*
  allocate(size_t size_, size_t align_)
  {
    return ::operator new(size_, std::align_val_t{align_});
  }

  void
  deallocate(void *ptr_, size_t /*size_*/, size_t align_)
  {
    ::operator delete(ptr_, std::align_val_t{align_});
  }
};

struct DefaultShouldPool
{
  template<typename T>
  bool operator()(T*) const noexcept
  {
    return true;
  }
};

template<typename T,
         typename Allocator = DefaultAllocator,
         typename ShouldPool = DefaultShouldPool>
class ObjPool
{
private:
  struct alignas(alignof(T)) Node
  {
    Node *next;
  };

private:
  static_assert(sizeof(T) >= sizeof(Node), "T must be at least pointer-sized");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

private:
  static
  Node*
  to_node(T *obj_)
  {
    return reinterpret_cast<Node*>(obj_);
  }

private:
  mutex_t _mtx;
  Node *_head = nullptr;
  std::atomic<size_t> _pool_count{0};
  Allocator _allocator;
  ShouldPool _should_pool;

public:
  template<typename AllocPred,
           typename = std::enable_if_t<
             !std::is_same_v<std::decay_t<AllocPred>, ObjPool> &&
             !std::is_same_v<std::decay_t<AllocPred>, Allocator>
           >>
  explicit
  ObjPool(AllocPred&& allocator_pred_)
    : _allocator(allocator_pred_)
  {}

  template<typename ShouldPoolPred,
           typename = std::enable_if_t<
             !std::is_same_v<std::decay_t<ShouldPoolPred>, ObjPool> &&
             !std::is_same_v<std::decay_t<ShouldPoolPred>, ShouldPool>
           >>
  explicit
  ObjPool(Allocator allocator_, ShouldPoolPred&& should_pool_pred_)
    : _allocator(allocator_),
      _should_pool(std::forward<ShouldPoolPred>(should_pool_pred_))
  {}

  ObjPool() = default;
  ObjPool(const ObjPool&) = delete;
  ObjPool& operator=(const ObjPool&) = delete;
  ObjPool(ObjPool&&) = delete;
  ObjPool& operator=(ObjPool&&) = delete;

  ~ObjPool()
  {
    clear();
  }

private:
  Node*
  _pop_node()
  {
    mutex_lock(&_mtx);

    Node *node = _head;
    if(node)
      {
        _head = node->next;
        _pool_count.fetch_sub(1, std::memory_order_relaxed);
      }

    mutex_unlock(&_mtx);

    return node;
  }

  void
  _push_node(Node *node_)
  {
    mutex_lock(_mtx);

    node_->next = _head;
    _head = node_;

    mutex_unlock(_mtx);

    _pool_count.fetch_add(1, std::memory_order_relaxed);
  }

public:
  void
  clear() noexcept
  {
    Node *head;

    mutex_lock(_mtx);

    head  = _head;
    _head = nullptr;

    mutex_unlock(_mtx);

    _pool_count.store(0, std::memory_order_relaxed);

    while(head)
      {
        Node *next = head->next;
        _allocator.deallocate(head, sizeof(Node), alignof(Node));
        head = next;
      }
  }

  template<typename... Args>
  T*
  alloc(size_t size_ = sizeof(T), Args&&... args_)
  {
    void *mem;
    Node *node;

    node = _pop_node();
    mem  = (node ?
            static_cast<void*>(node) :
            _allocator.allocate(size_, alignof(T)));

    try
      {
        return new(mem) T(std::forward<Args>(args_)...);
      }
    catch(...)
      {
        if(!node)
          _allocator.deallocate(mem, size_, alignof(T));
        throw;
      }
  }

  void
  free(T *obj_, size_t size_ = sizeof(T)) noexcept
  {
    if(not obj_)
      return;

    bool should_pool = _should_pool(obj_);

    obj_->~T();

    if(should_pool)
      _push_node(to_node(obj_));
    else
      _allocator.deallocate(obj_, size_, alignof(T));
  }

  size_t
  size() const noexcept
  {
    return _pool_count.load(std::memory_order_relaxed);
  }

  void
  gc() noexcept
  {
    size_t count = _pool_count.load(std::memory_order_relaxed);
    size_t to_free = std::max(count / 10, size_t{1});

    for(size_t i = 0; i < to_free; ++i)
      {
        Node *node = _pop_node();
        if(not node)
          break;
        _allocator.deallocate(node, sizeof(Node), alignof(Node));
      }
  }
};

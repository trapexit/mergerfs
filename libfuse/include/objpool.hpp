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

#include <algorithm>
#include <atomic>
#include <mutex>
#include <new>
#include <utility>
#include <type_traits>

template<typename T>
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
  std::mutex _mtx;
  Node *_head = nullptr;
  std::atomic<size_t> _pooled_count{0};

public:
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
    std::lock_guard<std::mutex> lock(_mtx);

    Node *node = _head;
    if(node)
      {
        _head = node->next;
        _pooled_count.fetch_sub(1, std::memory_order_relaxed);
      }

    return node;
  }

  void
  _push_node(Node *node_)
  {
    std::lock_guard<std::mutex> lock(_mtx);

    node_->next = _head;
    _head = node_;
    _pooled_count.fetch_add(1, std::memory_order_relaxed);
  }

public:
  void
  clear() noexcept
  {
    Node *head;

    {
      std::lock_guard<std::mutex> lock(_mtx);
      head  = _head;
      _head = nullptr;
      _pooled_count.store(0, std::memory_order_relaxed);
    }

    while(head)
      {
        Node *next = head->next;
        ::operator delete(head, std::align_val_t{alignof(T)});
        head = next;
      }
  }

  template<typename... Args>
  T*
  alloc(Args&&... args_)
  {
    void *mem;
    Node *node;

    node = _pop_node();
    mem  = (node ?
            static_cast<void*>(node) :
            ::operator new(sizeof(T), std::align_val_t{alignof(T)}));

    try
      {
        return new(mem) T(std::forward<Args>(args_)...);
      }
    catch(...)
      {
        _push_node(static_cast<Node*>(mem));
        throw;
      }
  }

  void
  free(T *obj_) noexcept
  {
    if(not obj_)
      return;

    obj_->~T();

    _push_node(to_node(obj_));
  }

  size_t
  size() const noexcept
  {
    return _pooled_count.load(std::memory_order_relaxed);
  }

  void
  gc() noexcept
  {
    size_t count = _pooled_count.load(std::memory_order_relaxed);
    size_t to_free = std::max(count / 10, size_t{1});

    for(size_t i = 0; i < to_free; ++i)
      {
        Node *node = _pop_node();
        if(not node)
          break;
        ::operator delete(node, std::align_val_t{alignof(T)});
      }
  }
};

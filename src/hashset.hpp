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
#include "rapidhash/rapidhash.h"

#include <cstring>


// Stores 64-bit hashes only, not keys. Relies on collision
// probability being extremely low at 64-bit width.
class HashSet
{
private:
  static constexpr size_t INLINE_CAPACITY = 8;
  static constexpr size_t INITIAL_CAPACITY = 128;

private:
  u64     _inline[INLINE_CAPACITY] = {};
  u64    *_table    = nullptr;
  size_t  _size     = 0;
  size_t  _capacity = 0;
  size_t  _mask     = 0;

public:
  HashSet() = default;

  ~HashSet()
  {
    delete[] _table;
  }

  HashSet(const HashSet&) = delete;
  HashSet& operator=(const HashSet&) = delete;

  inline
  int
  put(const char     *str_,
      cu64             len_)
  {
    u64 h;

    h = rapidhash(str_,len_);
    if(h == 0)
      h = 1;

    return _put_hash(h);
  }

  inline
  int
  put(const char *str_)
  {
    return put(str_,strlen(str_));
  }

  inline
  int
  size(void) const
  {
    return _size;
  }

private:
  inline
  int
  _put_hash(const u64 h_)
  {
    if(_table == nullptr)
      {
        for(size_t i = 0; i < _size; ++i)
          {
            if(_inline[i] == h_)
              return 0;
          }

        if(_size < INLINE_CAPACITY)
          {
            _inline[_size++] = h_;
            return 1;
          }

        _grow(INITIAL_CAPACITY);
      }

    // 75% was benchmarked to be best generally
    if(((_size + 1) * 4) >= (_capacity * 3))
      _grow(_capacity * 2);

    size_t idx = h_ & _mask;
    while(true)
      {
        const u64 cur = _table[idx];

        if(cur == 0)
          {
            _table[idx] = h_;
            ++_size;
            return 1;
          }

        if(cur == h_)
          return 0;

        idx = ((idx + 1) & _mask);
      }
  }

  inline
  void
  _grow(size_t min_capacity_)
  {
    size_t cap = 1;
    u64 *table;
    size_t mask;

    while(cap < min_capacity_)
      cap <<= 1;

    table = new u64[cap]();
    mask = cap - 1;

    if(_table == nullptr)
      {
        for(size_t i = 0; i < _size; ++i)
          _insert_existing(table,
                           mask,
                           _inline[i]);
      }
    else
      {
        for(size_t i = 0; i < _capacity; ++i)
          {
            const u64 h = _table[i];
            if(h == 0)
              continue;

            _insert_existing(table,
                             mask,
                             h);
          }

        delete[] _table;
      }

    _table = table;
    _capacity = cap;
    _mask = mask;
  }

  static
  inline
  void
  _insert_existing(u64    *table_,
                   size_t  mask_,
                   u64     h_)
  {
    size_t idx = h_ & mask_;

    while(table_[idx] != 0)
      idx = ((idx + 1) & mask_);

    table_[idx] = h_;
  }
};

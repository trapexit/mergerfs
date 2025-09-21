/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <cstdint>

#include <stdlib.h>


typedef struct fixed_mem_pool_t fixed_mem_pool_t;
struct fixed_mem_pool_t
{
  fixed_mem_pool_t *next;
};

template<uint64_t SIZE>
class FixedMemPool
{
public:
  FixedMemPool()
  {
    list.next = NULL;
  }

  ~FixedMemPool()
  {
    void *mem;
    while(!empty())
      {
        mem = alloc();
        ::free(mem);
      }
  }

  bool
  empty(void)
  {
    return (list.next == NULL);
  }

  uint64_t
  size(void)
  {
    return SIZE;
  }

  void*
  alloc(void)
  {
    void *rv;

    if(list.next == NULL)
      return malloc(SIZE);

    rv = (void*)list.next;
    list.next = list.next->next;

    return rv;
  }

  void
  free(void *mem_)
  {
    fixed_mem_pool_t *next;

    next = (fixed_mem_pool_t*)mem_;
    next->next = list.next;
    list.next = next;
  }

private:
  fixed_mem_pool_t list;
};

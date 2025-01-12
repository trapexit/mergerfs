/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#define RAPIDHASH_FAST
#define RAPIDHASH_UNROLLED

#include "khash.h"
#include "wyhash.h"
#include "rapidhash.h"
#include <cstdint>

static
inline
uint64_t
vt_hash_u64(const uint64_t hash_)
{
  return hash_;
}

static
inline
bool
vt_cmpr_u64(const uint64_t key_1,
            const uint64_t key_2)
{
  return (key_1 == key_2);
}

#define NAME u64_set
#define KEY_TY uint64_t
#define HASH_FN vt_hash_u64
#define CMPR_FN vt_cmpr_u64
#include "verstable.h"

KHASH_SET_INIT_INT64(hashset);


class HashSet
{
public:
  HashSet()
  {
    _set = kh_init(hashset);
    u64_set_init(&_u64_set);
  }

  ~HashSet()
  {
    kh_destroy(hashset,_set);
    u64_set_cleanup(&_u64_set);
  }

  inline
  int
  put(const char     *str_,
      const uint64_t  len_)
  {
    uint64_t hashval;
    size_t size;
    str_set_itr itr;

    size = u64_set_size(&_u64_set);
    itr = u64_set_get_or_insert(&_u64_set,hashval);

    return (size == size);
    
    int rv;
    uint64_t h;
    khint_t key;

    h = wyhash(str_,len_,0x7472617065786974,_wyp);

    key = kh_put(hashset,_set,h,&rv);
    if(rv == 0)
      return 0;

    kh_key(_set,key) = h;

    return rv;
  }

  inline
  int
  put(const char *str_)
  {
    return put(str_,strlen(str_));
  }

  inline
  int
  size(void)
  {
    return str_set_size(&_str_set);
    return kh_size(_set);
  }

private:
  khash_t(hashset) *_set;
  u64_set _u64_set;
};

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

#include "khash.h"
#include "wyhash.h"
#include <cstdint>

#define NAME str_set
#define KEY_TY const char*
#define HASH_FN vt_hash_string
#define CMPR_FN vt_cmpr_string
#include "verstable.h"

KHASH_SET_INIT_INT64(hashset);

static
inline
uint64_t
vt_hash_const_string(const char *key_)
{
  uint64_t hash = 0xcbf29ce484222325ull;

  while( *key )
    hash = ( (unsigned char)*key++ ^ hash ) * 0x100000001b3ull;

  return hash;  
}



class HashSet
{
public:
  HashSet()
  {
    _set = kh_init(hashset);
    str_set_init(&_str_set);
  }

  ~HashSet()
  {
    kh_destroy(hashset,_set);
    str_set_cleanup(&_str_set);
  }

  inline
  int
  put(const char     *str_,
      const uint64_t  len_)
  {
    size_t size;
    str_set_itr itr;

    size = 
    itr = str_set_get_or_insert(&_str_set,str_);

    
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
  str_set _str_set;
};

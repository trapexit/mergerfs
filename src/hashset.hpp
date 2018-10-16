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
#include "fasthash.h"

KHASH_SET_INIT_INT64(hashset);

class HashSet
{
public:
  HashSet()
  {
    _set = kh_init(hashset);
  }

  ~HashSet()
  {
    kh_destroy(hashset,_set);
  }

  inline
  int
  put(const char *str_)
  {
    int rv;
    uint64_t h;
    khint_t key;

    h = fasthash64(str_,strlen(str_),0x7472617065786974);

    key = kh_put(hashset,_set,h,&rv);
    if(rv == 0)
      return 0;

    kh_key(_set,key) = h;

    return rv;
  }

  inline
  int
  size(void)
  {
    return kh_size(_set);
  }

private:
  khash_t(hashset) *_set;
};

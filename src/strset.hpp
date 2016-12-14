/*
  ISC License

  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#ifndef __STRSET_HPP__
#define __STRSET_HPP__

#include <string.h>

#include "khash.h"

KHASH_SET_INIT_STR(strset);

class StrSet
{
public:
  StrSet()
  {
    _set = kh_init(strset);
  }

  ~StrSet()
  {
    for(khint_t k = kh_begin(_set), ek = kh_end(_set); k != ek; k++)
      if(kh_exist(_set,k))
        free((char*)kh_key(_set,k));

    kh_destroy(strset,_set);
  }

  inline
  int
  put(const char *str)
  {
    int rv;
    khint_t key;

    key = kh_put(strset,_set,str,&rv);
    if(rv == 0)
      return 0;

    kh_key(_set,key) = strdup(str);

    return rv;
  }

private:
  khash_t(strset) *_set;
};

#endif

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

#define STATIC_ASSERT(cond) assert::StaticAssert< (cond) >()
#define STATIC_ARRAYLENGTH_ASSERT(array,size) STATIC_ASSERT(((sizeof(array)/sizeof(array[0]))==(size)))

namespace assert
{
  template<bool>
  struct StaticAssert;

  template<>
  struct StaticAssert<true>
  {};
}

#ifdef NDEBUG

#define ASSERT_NOT_NULL(ptr)

#else

#include <cassert>
#include <iostream>

#define ASSERT_NOT_NULL(ptr)                            \
  do {                                                  \
    auto *_tmp = (ptr);                                 \
    if((_tmp) == nullptr) {                             \
      std::cerr << "ASSERT_NOT_NULL failed\n"           \
                << "  expression: \"" #ptr "\"\n"       \
                << "  file: " << __FILE__ << "\n"       \
                << "  line: " << __LINE__ << "\n"       \
                << "  function: " << __func__ << "\n";  \
      assert((_tmp) != nullptr);                        \
    }                                                   \
  } while (0)

#endif

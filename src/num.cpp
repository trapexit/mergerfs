/*
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

#include "ef.hpp"

#include "fmt/core.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cstdint>


#define KB (1024ULL)
#define MB (KB * 1024ULL)
#define GB (MB * 1024ULL)
#define TB (GB * 1024ULL)


namespace num
{
  std::string
  humanize(const uint64_t bytes_)
  {
    if(bytes_ < KB)
      return fmt::format("{}",bytes_);
    ef(((bytes_ / TB) * TB) == bytes_)
      return fmt::format("{}T",bytes_ / TB);
    ef(((bytes_ / GB) * GB) == bytes_)
      return fmt::format("{}G",bytes_ / GB);
    ef(((bytes_ / MB) * MB) == bytes_)
      return fmt::format("{}M",bytes_ / MB);
    ef(((bytes_ / KB) * KB) == bytes_)
      return fmt::format("{}K",bytes_ / KB);

    return fmt::format("{}",bytes_);
  }
}

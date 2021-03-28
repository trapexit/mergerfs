/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <cstdint>
#include <string>

#define KB (1024UL)
#define MB (KB * 1024UL)
#define GB (MB * 1024UL)
#define TB (GB * 1024UL)

namespace humanize
{
  int
  from(const std::string &str_,
       uint64_t          *u_)
  {
    char *endptr;
    uint64_t tmp;

    tmp = ::strtoll(str_.c_str(),&endptr,10);
    switch(*endptr)
      {
      case 'k':
      case 'K':
        tmp *= 1024ULL;
        break;

      case 'm':
      case 'M':
        tmp *= (1024ULL * 1024ULL);
        break;

      case 'g':
      case 'G':
        tmp *= (1024ULL * 1024ULL * 1024ULL);
        break;

      case 't':
      case 'T':
        tmp *= (1024ULL * 1024ULL * 1024ULL * 1024ULL);
        break;

      case '\0':
        break;

      default:
        return -EINVAL;
      }

    *u_ = tmp;

    return 0;
  }

  std::string
  to(const uint64_t u_)
  {
    char buf[64];

    if(u_ < KB)
      sprintf(buf,"%lu",u_);
    ef(((u_ / TB) * TB) == u_)
      sprintf(buf,"%luT",u_ / TB);
    ef(((u_ / GB) * GB) == u_)
      sprintf(buf,"%luG",u_ / GB);
    ef(((u_ / MB) * MB) == u_)
      sprintf(buf,"%luM",u_ / MB);
    ef(((u_ / KB) * KB) == u_)
      sprintf(buf,"%luK",u_ / KB);
    else
      sprintf(buf,"%lu",u_);

    return std::string(buf);
  }
}

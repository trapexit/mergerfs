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

#include <cstdint>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define KB (1024UL)
#define MB (KB * 1024UL)
#define GB (MB * 1024UL)
#define TB (GB * 1024UL)


namespace num
{
  int
  to_uint64_t(const std::string &str,
              uint64_t          &value)
  {
    char *endptr;
    uint64_t tmp;

    tmp = strtoll(str.c_str(),&endptr,10);
    switch(*endptr)
      {
      case 'k':
      case 'K':
        tmp *= 1024;
        break;

      case 'm':
      case 'M':
        tmp *= (1024 * 1024);
        break;

      case 'g':
      case 'G':
        tmp *= (1024 * 1024 * 1024);
        break;

      case 't':
      case 'T':
        tmp *= (1024ULL * 1024 * 1024 * 1024);
        break;

      case '\0':
        break;

      default:
        return -1;
      }

    value = tmp;

    return 0;
  }

  int
  to_double(const std::string &str_,
            double            *d_)
  {
    double tmp;
    char *endptr;

    tmp = strtod(str_.c_str(),&endptr);
    if(*endptr != '\0')
      return -1;

    *d_ = tmp;

    return 0;
  }

  int
  to_time_t(const std::string &str,
            time_t            &value)
  {
    time_t tmp;
    char *endptr;

    tmp = strtoll(str.c_str(),&endptr,10);
    if(*endptr != '\0')
      return -1;
    if(tmp < 0)
      return -1;

    value = tmp;

    return 0;
  }
}

 namespace num
{
  std::string
  humanize(const uint64_t bytes_)
  {
    char buf[64];

    if(bytes_ < KB)
      sprintf(buf,"%lu",bytes_);
    ef(((bytes_ / TB) * TB) == bytes_)
      sprintf(buf,"%luT",bytes_ / TB);
    ef(((bytes_ / GB) * GB) == bytes_)
      sprintf(buf,"%luG",bytes_ / GB);
    ef(((bytes_ / MB) * MB) == bytes_)
      sprintf(buf,"%luM",bytes_ / MB);
    ef(((bytes_ / KB) * KB) == bytes_)
      sprintf(buf,"%luK",bytes_ / KB);
    else
      sprintf(buf,"%lu",bytes_);

    return std::string(buf);
  }
}

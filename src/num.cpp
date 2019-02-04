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

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <string>

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

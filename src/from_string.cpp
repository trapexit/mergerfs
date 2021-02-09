/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "errno.hpp"

#include <cstdint>
#include <string>

#include <stdlib.h>


namespace str
{
  int
  from(const std::string &value_,
       bool              *bool_)
  {
    if((value_ == "true") ||
       (value_ == "1")    ||
       (value_ == "on")   ||
       (value_ == "yes"))
      *bool_ = true;
    ef((value_ == "false") ||
       (value_ == "0")     ||
       (value_ == "off")   ||
       (value_ == "no"))
      *bool_ = false;
    else
      return -EINVAL;

    return 0;
  }

  int
  from(const std::string &value_,
       int               *int_)
  {
    int tmp;
    char *endptr;

    errno = 0;
    tmp = ::strtol(value_.c_str(),&endptr,10);
    if(errno != 0)
      return -EINVAL;
    if(endptr == value_.c_str())
      return -EINVAL;

    *int_ = tmp;

    return 0;
  }

  int
  from(const std::string &value_,
       uint64_t          *uint64_)
  {
    char *endptr;
    uint64_t tmp;

    tmp = ::strtoll(value_.c_str(),&endptr,10);
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

    *uint64_ = tmp;

    return 0;
  }

  int
  from(const std::string &value_,
       std::string       *str_)
  {
    *str_ = value_;

    return 0;
  }

  int
  from(const std::string &value_,
       const std::string *key_)
  {
    return -EINVAL;
  }
}

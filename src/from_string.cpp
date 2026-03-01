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

#include "from_string.hpp"

#include "ef.hpp"
#include "errno.hpp"

#include <charconv>

#include <stdlib.h>


int
str::from(const std::string_view  value_,
          bool                   *bool_)
{
  if((value_ == "true") ||
     (value_ == "t")    ||
     (value_ == "1")    ||
     (value_ == "on")   ||
     (value_ == "yes"))
    *bool_ = true;
  ef((value_ == "false") ||
     (value_ == "f")     ||
     (value_ == "0")     ||
     (value_ == "off")   ||
     (value_ == "no"))
    *bool_ = false;
  else
    return -EINVAL;

  return 0;
}

int
str::from(const std::string_view  val_,
          int                    *rv_)
{
  int tmp;
  const int base = 10;

  auto [ptr,ec] = std::from_chars(val_.begin(),
                                  val_.end(),
                                  tmp,
                                  base);
  if(ec != std::errc{})
    return -EINVAL;

  *rv_ = tmp;

  return 0;
}

int
str::from(const std::string_view  val_,
           s64                    *rv_)
{
  s64 tmp;
  const int base = 10;
  constexpr s64 K = 1024LL;
  constexpr s64 M = K * 1024LL;
  constexpr s64 G = M * 1024LL;
  constexpr s64 T = G * 1024LL;

  auto [ptr,ec] = std::from_chars(val_.begin(),
                                  val_.end(),
                                  tmp,
                                  base);

  if(ec != std::errc{})
    return -EINVAL;
  if(ptr == val_.end())
    {
      *rv_ = tmp;
      return 0;
    }

  switch(*ptr)
    {
    case 'b':
    case 'B':
      break;

    case 'k':
    case 'K':
      if(__builtin_mul_overflow(tmp,K,&tmp))
        return -EOVERFLOW;
      break;

    case 'm':
    case 'M':
      if(__builtin_mul_overflow(tmp,M,&tmp))
        return -EOVERFLOW;
      break;

    case 'g':
    case 'G':
      if(__builtin_mul_overflow(tmp,G,&tmp))
        return -EOVERFLOW;
      break;

    case 't':
    case 'T':
      if(__builtin_mul_overflow(tmp,T,&tmp))
        return -EOVERFLOW;
      break;

    default:
      return -EINVAL;
    }

  *rv_ = tmp;
  return 0;
}

int
str::from(const std::string_view  val_,
           u64                    *rv_)
{
  u64 tmp;
  const int base = 10;
  constexpr u64 K = 1024ULL;
  constexpr u64 M = K * 1024ULL;
  constexpr u64 G = M * 1024ULL;
  constexpr u64 T = G * 1024ULL;

  auto [ptr,ec] = std::from_chars(val_.begin(),
                                  val_.end(),
                                  tmp,
                                  base);

  if(ec != std::errc{})
    return -EINVAL;
  if(ptr == val_.end())
    {
      *rv_ = tmp;
      return 0;
    }

  switch(*ptr)
    {
    case 'b':
    case 'B':
      break;

    case 'k':
    case 'K':
      if(__builtin_mul_overflow(tmp,K,&tmp))
        return -EOVERFLOW;
      break;

    case 'm':
    case 'M':
      if(__builtin_mul_overflow(tmp,M,&tmp))
        return -EOVERFLOW;
      break;

    case 'g':
    case 'G':
      if(__builtin_mul_overflow(tmp,G,&tmp))
        return -EOVERFLOW;
      break;

    case 't':
    case 'T':
      if(__builtin_mul_overflow(tmp,T,&tmp))
        return -EOVERFLOW;
      break;

    default:
      return -EINVAL;
    }

  *rv_ = tmp;
  return 0;
}

int
str::from(const std::string_view  value_,
          std::string            *str_)
{
  *str_ = value_;

  return 0;
}

int
str::from(const std::string_view  value_,
          const std::string      *key_)
{
  return -EINVAL;
}

int
str::from(const std::string_view  value_,
          fs::path               *path_)
{
  *path_ = value_;

  return 0;
}

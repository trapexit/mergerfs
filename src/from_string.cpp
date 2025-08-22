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
          int64_t                *rv_)
{
  int64_t tmp;
  const int base = 10;

  auto [ptr,ec] = std::from_chars(val_.begin(),
                                  val_.end(),
                                  tmp,
                                  base);

  if(ec != std::errc{})
    return -EINVAL;
  *rv_ = tmp;
  if(ptr == val_.end())
    return 0;

  switch(*ptr)
    {
    case 'b':
    case 'B':
      *rv_ *= 1ULL;
      break;

    case 'k':
    case 'K':
      *rv_ *= 1024ULL;
      break;

    case 'm':
    case 'M':
      *rv_ *= (1024ULL * 1024ULL);
      break;

    case 'g':
    case 'G':
      *rv_ *= (1024ULL * 1024ULL * 1024ULL);
      break;

    case 't':
    case 'T':
      *rv_ *= (1024ULL * 1024ULL * 1024ULL * 1024ULL);
      break;

    default:
      return -EINVAL;
    }

  return 0;
}

int
str::from(const std::string_view  val_,
          uint64_t               *rv_)
{
  uint64_t tmp;
  const int base = 10;

  auto [ptr,ec] = std::from_chars(val_.begin(),
                                  val_.end(),
                                  tmp,
                                  base);

  if(ec != std::errc{})
    return -EINVAL;
  *rv_ = tmp;
  if(ptr == val_.end())
    return 0;

  switch(*ptr)
    {
    case 'b':
    case 'B':
      *rv_ *= 1ULL;
      break;

    case 'k':
    case 'K':
      *rv_ *= 1024ULL;
      break;

    case 'm':
    case 'M':
      *rv_ *= (1024ULL * 1024ULL);
      break;

    case 'g':
    case 'G':
      *rv_ *= (1024ULL * 1024ULL * 1024ULL);
      break;

    case 't':
    case 'T':
      *rv_ *= (1024ULL * 1024ULL * 1024ULL * 1024ULL);
      break;

    default:
      return -EINVAL;
    }

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
          std::filesystem::path  *path_)
{
  *path_ = value_;

  return 0;
}

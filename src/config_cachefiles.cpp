/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config_cachefiles.hpp"
#include "ef.hpp"
#include "errno.hpp"

template<>
std::string
CacheFiles::to_string() const
{
  switch(_data)
    {
    case CacheFiles::ENUM::LIBFUSE:
      return "libfuse";
    case CacheFiles::ENUM::OFF:
      return "off";
    case CacheFiles::ENUM::PARTIAL:
      return "partial";
    case CacheFiles::ENUM::FULL:
      return "full";
    case CacheFiles::ENUM::AUTO_FULL:
      return "auto-full";
    }

  return "invalid";
}

template<>
int
CacheFiles::from_string(const std::string &s_)
{
  if(s_ == "libfuse")
    _data = CacheFiles::ENUM::LIBFUSE;
  ef(s_ == "off")
    _data = CacheFiles::ENUM::OFF;
  ef(s_ == "partial")
    _data = CacheFiles::ENUM::PARTIAL;
  ef(s_ == "full")
    _data = CacheFiles::ENUM::FULL;
  ef(s_ == "auto-full")
    _data = CacheFiles::ENUM::AUTO_FULL;
  else
    return -EINVAL;

  return 0;
}

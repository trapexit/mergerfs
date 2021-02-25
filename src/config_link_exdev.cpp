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

#include "config_link_exdev.hpp"
#include "ef.hpp"
#include "errno.hpp"

template<>
std::string
LinkEXDEV::to_string(void) const
{
  switch(_data)
    {
    case LinkEXDEV::ENUM::PASSTHROUGH:
      return "passthrough";
    case LinkEXDEV::ENUM::REL_SYMLINK:
      return "rel-symlink";
    case LinkEXDEV::ENUM::ABS_BASE_SYMLINK:
      return "abs-base-symlink";
    case LinkEXDEV::ENUM::ABS_POOL_SYMLINK:
      return "abs-pool-symlink";
    }

  return "invalid";
}

template<>
int
LinkEXDEV::from_string(const std::string &s_)
{
  if(s_ == "passthrough")
    _data = LinkEXDEV::ENUM::PASSTHROUGH;
  ef(s_ == "rel-symlink")
    _data = LinkEXDEV::ENUM::REL_SYMLINK;
  ef(s_ == "abs-base-symlink")
    _data = LinkEXDEV::ENUM::ABS_BASE_SYMLINK;
  ef(s_ == "abs-pool-symlink")
    _data = LinkEXDEV::ENUM::ABS_POOL_SYMLINK;
  else
    return -EINVAL;

  return 0;
}

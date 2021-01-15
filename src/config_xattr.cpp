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

#include "config_xattr.hpp"
#include "ef.hpp"
#include "errno.hpp"


template<>
std::string
XAttr::to_string() const
{
  switch(_data)
    {
    case XAttr::ENUM::PASSTHROUGH:
      return "passthrough";
    case XAttr::ENUM::NOSYS:
      return "nosys";
    case XAttr::ENUM::NOATTR:
      return "noattr";
    }

  return "invalid";
}

template<>
int
XAttr::from_string(const std::string &s_)
{
  if(s_ == "passthrough")
    _data = XAttr::ENUM::PASSTHROUGH;
  ef(s_ == "nosys")
    _data = XAttr::ENUM::NOSYS;
  ef(s_ == "noattr")
    _data = XAttr::ENUM::NOATTR;
  else
    return -EINVAL;

  return 0;
}

/*
  ISC License

  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config_passthrough_io.hpp"
#include "ef.hpp"
#include "errno.hpp"

template<>
std::string
PassthroughIO::to_string() const
{
  switch(_data)
    {
    case PassthroughIO::ENUM::OFF:
      return "off";
    case PassthroughIO::ENUM::RO:
      return "ro";
    case PassthroughIO::ENUM::WO:
      return "wo";
    case PassthroughIO::ENUM::RW:
      return "rw";
    }

  return "invalid";
}

template<>
int
PassthroughIO::from_string(const std::string_view s_)
{
  if(s_ == "off")
    _data = PassthroughIO::ENUM::OFF;
  ef(s_ == "ro")
    _data = PassthroughIO::ENUM::RO;
  ef(s_ == "wo")
    _data = PassthroughIO::ENUM::WO;
  ef(s_ == "rw")
    _data = PassthroughIO::ENUM::RW;
  else
    return -EINVAL;

  return 0;
}

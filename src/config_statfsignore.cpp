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

#include "config_statfsignore.hpp"
#include "ef.hpp"
#include "errno.hpp"


template<>
std::string
StatFSIgnore::to_string() const
{
  switch(_data)
    {
    case StatFSIgnore::ENUM::NONE:
      return "none";
    case StatFSIgnore::ENUM::RO:
      return "ro";
    case StatFSIgnore::ENUM::NC:
      return "nc";
    }

  return "invalid";
}

template<>
int
StatFSIgnore::from_string(const std::string &s_)
{
  if(s_ == "none")
    _data = StatFSIgnore::ENUM::NONE;
  ef(s_ == "ro")
    _data = StatFSIgnore::ENUM::RO;
  ef(s_ == "nc")
    _data = StatFSIgnore::ENUM::NC;
  else
    return -EINVAL;

  return 0;
}

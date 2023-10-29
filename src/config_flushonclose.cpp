/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config_flushonclose.hpp"
#include "ef.hpp"
#include "errno.hpp"

template<>
std::string
FlushOnClose::to_string() const
{
  switch(_data)
    {
    case FlushOnClose::ENUM::NEVER:
      return "never";
    case FlushOnClose::ENUM::OPENED_FOR_WRITE:
      return "opened-for-write";
    case FlushOnClose::ENUM::ALWAYS:
      return "always";
    }

  return {};
}

template<>
int
FlushOnClose::from_string(const std::string &s_)
{
  if(s_ == "never")
    _data = FlushOnClose::ENUM::NEVER;
  ef(s_ == "opened-for-write")
    _data = FlushOnClose::ENUM::OPENED_FOR_WRITE;
  ef(s_ == "always")
    _data = FlushOnClose::ENUM::ALWAYS;
  else
    return -EINVAL;

  return 0;
}

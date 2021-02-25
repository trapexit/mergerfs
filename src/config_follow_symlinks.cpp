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

#include "config_follow_symlinks.hpp"
#include "ef.hpp"
#include "errno.hpp"

template<>
std::string
FollowSymlinks::to_string(void) const
{
  switch(_data)
    {
    case FollowSymlinks::ENUM::NEVER:
      return "never";
    case FollowSymlinks::ENUM::DIRECTORY:
      return "directory";
    case FollowSymlinks::ENUM::REGULAR:
      return "regular";
    case FollowSymlinks::ENUM::ALL:
      return "all";
    }

  return "invalid";
}

template<>
int
FollowSymlinks::from_string(const std::string &s_)
{
  if(s_ == "never")
    _data = FollowSymlinks::ENUM::NEVER;
  ef(s_ == "directory")
    _data = FollowSymlinks::ENUM::DIRECTORY;
  ef(s_ == "regular")
    _data = FollowSymlinks::ENUM::REGULAR;
  ef(s_ == "all")
    _data = FollowSymlinks::ENUM::ALL;
  else
    return -EINVAL;

  return 0;
}

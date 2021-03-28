/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

enum class RenameEXDEV
  {
   INVALID,
   PASSTHROUGH,
   REL_SYMLINK,
   ABS_SYMLINK
  };

namespace toml
{
  template<>
  struct from<RenameEXDEV>
  {
    static
    RenameEXDEV
    from_toml(const toml::value &v_)
    {
      const std::string &s = v_.as_string();

      if(s == "passthrough")
        return RenameEXDEV::PASSTHROUGH;
      if(s == "rel-symlink")
        return RenameEXDEV::REL_SYMLINK;
      if(s == "abs-symlink")
        return RenameEXDEV::ABS_SYMLINK;

      return RenameEXDEV::INVALID;
    }
  };
}

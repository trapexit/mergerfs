/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_path.hpp"

#include "toml.hpp"

#include <cstdint>
#include <string>
#include <vector>


class Branch
{
public:
  enum class Mode
    {
     INVALID,
     RO,
     RW,
     NC
    };

  static Mode str2mode(const std::string &);

public:
  Branch(const toml::value  &toml,
         const Branch::Mode  default_mode,
         const uint64_t      default_minfreespace);

public:
  bool ro(void) const;
  bool nc(void) const;
  bool ro_or_nc(void) const;

public:
  gfs::path path;
  Mode      mode;
  uint64_t  minfreespace;
};

namespace toml
{
  template<>
  struct from<Branch::Mode>
  {
    static
    Branch::Mode
    from_toml(const toml::value &v_)
    {
      std::string str = v_.as_string();

      if(str == "RW")
        return Branch::Mode::RW;
      if(str == "RO")
        return Branch::Mode::RO;
      if(str == "NC")
        return Branch::Mode::NC;

      return Branch::Mode::RW;
    }
  };
}

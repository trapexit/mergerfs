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

#include "branch.hpp"


Branch::Mode
Branch::str2mode(const std::string &str_)
{
  if(str_ == "RW")
    return Branch::Mode::RW;
  if(str_ == "RO")
    return Branch::Mode::RO;
  if(str_ == "NC")
    return Branch::Mode::NC;

  return Branch::Mode::RW;
}

Branch::Branch(const toml::value  &toml_,
               const Branch::Mode  default_mode_,
               const uint64_t      default_minfreespace_)
{
  mode         = toml::find_or(toml_,"mode",default_mode_);
  minfreespace = toml::find_or(toml_,"min-free-space",default_minfreespace_);
  path         = toml::find<std::string>(toml_,"path");
}

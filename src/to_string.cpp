/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "to_string.hpp"

#include "fmt/core.h"

#include <cstdint>
#include <string>


std::string
str::to(const bool bool_)
{
  return (bool_ ? "true" : "false");
}

std::string
str::to(const int int_)
{
  return fmt::format("{}",int_);
}

std::string
str::to(const uint32_t i_)
{
  return fmt::format("{}",i_);
}

std::string
str::to(const uint64_t uint64_)
{
  return fmt::format("{}",uint64_);
}

std::string
str::to(const int64_t int64_)
{
  return fmt::format("{}",int64_);
}

std::string
str::to(const std::string &s_)
{
  return s_;
}

std::string
str::to(const fs::path &path_)
{
  return path_.string();
}

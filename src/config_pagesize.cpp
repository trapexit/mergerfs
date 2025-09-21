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

#include "config_pagesize.hpp"

#include "from_string.hpp"

#include <cctype>
#include <string>

#include <unistd.h>


ConfigPageSize::ConfigPageSize(const uint64_t v_)
  : _v(v_)
{

}

ConfigPageSize::ConfigPageSize(const std::string &s_)
{
  from_string(s_);
}

std::string
ConfigPageSize::to_string(void) const
{
  return std::to_string(_v);
}

int
ConfigPageSize::from_string(const std::string_view s_)
{
  uint64_t v;
  uint64_t pagesize;

  pagesize = sysconf(_SC_PAGESIZE);

  str::from(s_,&v);
  if(!std::isalpha(s_.back()))
    v *= pagesize;

  _v = ((v + pagesize - 1) / pagesize);

  return 0;
}

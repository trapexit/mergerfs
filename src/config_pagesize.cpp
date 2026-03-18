/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fatal.hpp"
#include "from_string.hpp"

#include <cassert>
#include <climits>
#include <cctype>
#include <string>

#include <unistd.h>


ConfigPageSize::ConfigPageSize(cu64 v_)
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
  int rv;
  u64 v;
  long tmp;
  u64 pagesize;

  if(s_.empty())
    return -EINVAL;

  tmp = sysconf(_SC_PAGESIZE);
  if(tmp <= 0)
    fatal::abort("pagesize query failed - {}",strerror(errno));

  pagesize = (u64)tmp;

  rv = str::from(s_,&v);
  if(rv < 0)
    return rv;

  if(!std::isalpha(s_.back()))
    v *= pagesize;

  _v = ((v + pagesize - 1) / pagesize);

  return 0;
}

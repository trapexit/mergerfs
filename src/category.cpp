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

#include "category.hpp"
#include "errno.hpp"

#include <set>
#include <string>

int
Category::Base::from_string(const std::string_view s_)
{
  int rv;

  for(auto func : funcs)
    {
      rv = func->from_string(s_);
      if(rv < 0)
        return rv;
    }

  {
    std::lock_guard<std::mutex> lk(_last_set_mutex);
    _last_set = std::string(s_);
  }

  return 0;
}

std::string
Category::Base::to_string(void) const
{
  // Snapshot the last category-wide input under the lock so a
  // concurrent from_string can't tear the string mid-read.
  std::string last;
  {
    std::lock_guard<std::mutex> lk(_last_set_mutex);
    last = _last_set;
  }

  // Collect current constituent names. Each FuncWrapper::to_string is
  // itself mutex-protected.
  std::set<std::string> names;
  for(const auto func : funcs)
    names.insert(func->to_string());

  if(names.size() == 1)
    {
      // All children agree. Prefer _last_set (round-trippable through
      // the category-wide setter even when it maps to the agreed
      // name); otherwise fall back to the agreed name.
      if(!last.empty())
        return last;
      return *names.begin();
    }

  // Children diverge — either no category-wide assignment ever ran,
  // or a per-function override has changed a child since the last
  // category-wide set. Report "mixed" rather than a stale _last_set,
  // since per-function .to_string()s are the source of truth for the
  // actual policy in effect.
  return std::string("mixed");
}

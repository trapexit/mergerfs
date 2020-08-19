/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "errno.hpp"
#include "policy.hpp"

#include <algorithm>
#include <string>
#include <vector>

using std::string;
using std::vector;

int
Policy::Func::eprand(const Category  type_,
                     const Branches &branches_,
                     const char     *fusepath_,
                     const uint64_t  minfreespace_,
                     vector<string> *paths_)
{
  int rv;

  rv = Policy::Func::epall(type_,branches_,fusepath_,minfreespace_,paths_);
  if(rv == 0)
    {
      std::random_shuffle(paths_->begin(),paths_->end());
      paths_->resize(1);
    }

  return rv;
}

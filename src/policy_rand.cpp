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
#include "policy_rand.hpp"
#include "policies.hpp"

#include <algorithm>

int
Policy::Rand::Action::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  int rv;

  rv = Policies::Action::all(branches_,fusepath_,paths_);
  if(rv == 0)
    {
      std::random_shuffle(paths_->begin(),paths_->end());
      paths_->resize(1);
    }

  return rv;
}

int
Policy::Rand::Create::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  int rv;

  rv = Policies::Create::all(branches_,fusepath_,paths_);
  if(rv == 0)
    {
      std::random_shuffle(paths_->begin(),paths_->end());
      paths_->resize(1);
    }

  return rv;
}

int
Policy::Rand::Search::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  int rv;

  rv = Policies::Search::all(branches_,fusepath_,paths_);
  if(rv == 0)
    {
      std::random_shuffle(paths_->begin(),paths_->end());
      paths_->resize(1);
    }

  return rv;
}

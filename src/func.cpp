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

#include "func.hpp"


int
Func::Base::Action::from_string(const std::string_view policyname_)
{
  policy = Policies::Action::find(policyname_);
  if(!policy)
    return -EINVAL;

  return 0;
}

std::string
Func::Base::Action::to_string(void) const
{
  return policy.name();
}

int
Func::Base::Create::from_string(const std::string_view policyname_)
{
  policy = Policies::Create::find(policyname_);
  if(!policy)
    return -EINVAL;

  return 0;
}

std::string
Func::Base::Create::to_string(void) const
{
  return policy.name();
}

int
Func::Base::Search::from_string(const std::string_view policyname_)
{
  policy = Policies::Search::find(policyname_);
  if(!policy)
    return -EINVAL;

  return 0;
}

std::string
Func::Base::Search::to_string(void) const
{
  return policy.name();
}

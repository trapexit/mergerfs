/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "policy.hpp"

namespace Policy
{
  namespace FF
  {
    class Action final : public Policy::ActionImpl
    {
    public:
      Action()
        : Policy::ActionImpl("ff")
      {}

    public:
      int operator()(const Branches::Ptr&,
                     const char*,
                     std::vector<Branch*>&) const final;
    };

    class Create final : public Policy::CreateImpl
    {
    public:
      Create()
        : Policy::CreateImpl("ff")
      {}

    public:
      bool path_preserving(void) const final { return false; }
      int operator()(const Branches::Ptr&,
                     const char*,
                     std::vector<Branch*>&) const final;

    };

    class Search final : public Policy::SearchImpl
    {
    public:
      Search()
        : Policy::SearchImpl("ff")
      {}

    public:
      int operator()(const Branches::Ptr&,
                     const char*,
                     std::vector<Branch*>&) const final;
    };
  }
}

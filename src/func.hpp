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

#pragma once

#include "policy.hpp"
#include "policies.hpp"
#include "tofrom_string.hpp"

#include <string>


namespace Func
{
  namespace Base
  {
    class Action : public ToFromString
    {
    public:
      Action(Policy::ActionImpl *policy_)
        : policy(policy_)
      {}

    public:
      int from_string(const std::string &s) final;
      std::string to_string() const final;

    public:
      Policy::Action policy;
    };

    class Create : public ToFromString
    {
    public:
      Create(Policy::CreateImpl *policy_)
        : policy(policy_)
      {}

    public:
      int from_string(const std::string &s) final;
      std::string to_string() const final;

    public:
      Policy::Create policy;
    };

    class Search : public ToFromString
    {
    public:
      Search(Policy::SearchImpl *policy_)
        : policy(policy_)
      {}

    public:
      int from_string(const std::string &s) final;
      std::string to_string() const final;

    public:
      Policy::Search policy;
    };

    class ActionDefault : public Action
    {
    public:
      ActionDefault()
        : Func::Base::Action(&Policies::Action::epall)
      {
      }
    };

    class CreateDefault : public Create
    {
    public:
      CreateDefault()
        : Func::Base::Create(&Policies::Create::epmfs)
      {
      }
    };

    class SearchDefault : public Search
    {
    public:
      SearchDefault()
        : Func::Base::Search(&Policies::Search::ff)
      {
      }
    };
  }

  class Access final : public Base::SearchDefault
  {
  };

  class Chmod final : public Base::ActionDefault
  {
  };

  class Chown final : public Base::ActionDefault
  {
  };

  class Create final : public Base::CreateDefault
  {
  };

  class GetAttr final : public Base::SearchDefault
  {
  };

  class GetXAttr final : public Base::SearchDefault
  {
  };

  class Link final : public Base::ActionDefault
  {
  };

  class ListXAttr final : public Base::SearchDefault
  {
  };

  class Mkdir final : public Base::CreateDefault
  {
  };

  class Mknod final : public Base::CreateDefault
  {
  };

  class Open final : public Base::SearchDefault
  {
  };

  class Readlink final : public Base::SearchDefault
  {
  };

  class RemoveXAttr final : public Base::ActionDefault
  {
  };

  class Rename final : public Base::ActionDefault
  {
  };

  class Rmdir final : public Base::ActionDefault
  {
  };

  class SetXAttr final : public Base::ActionDefault
  {
  };

  class Symlink final : public Base::CreateDefault
  {
  };

  class Truncate final : public Base::ActionDefault
  {
  };

  class Unlink final : public Base::ActionDefault
  {
  };

  class Utimens final : public Base::ActionDefault
  {
  };
}

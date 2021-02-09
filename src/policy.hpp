/*
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

#include "branches.hpp"
#include "strvec.hpp"

#include <string>

namespace Policy
{
  class ActionImpl
  {
  public:
    ActionImpl(const std::string &name_)
      : name(name_)
    {
    }

  public:
    std::string name;
    virtual int operator()(const Branches::CPtr&,const char*,StrVec*) const = 0;
  };

  class Action
  {
  public:
    Action(ActionImpl *impl_)
      : impl(impl_)
    {}

    Action&
    operator=(ActionImpl *impl_)
    {
      impl = impl_;
      return *this;
    }

    const
    std::string&
    name(void) const
    {
      return impl->name;
    }

    int
    operator()(const Branches::CPtr &branches_,
               const char           *fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_,paths_);
    }

    int
    operator()(const Branches::CPtr &branches_,
               const std::string    &fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_.c_str(),paths_);
    }

  private:
    ActionImpl *impl;
  };

  class CreateImpl
  {
  public:
    CreateImpl(const std::string &name_)
      : name(name_)
    {
    }

  public:
    std::string name;
    virtual int operator()(const Branches::CPtr&,const char*,StrVec*) const = 0;
    virtual bool path_preserving(void) const = 0;
  };

  class Create
  {
  public:
    Create(CreateImpl *impl_)
      : impl(impl_)
    {}

    Create&
    operator=(CreateImpl *impl_)
    {
      impl = impl_;
      return *this;
    }

    const
    std::string&
    name(void) const
    {
      return impl->name;
    }

    bool
    path_preserving(void) const
    {
      return impl->path_preserving();
    }

    int
    operator()(const Branches::CPtr &branches_,
               const char           *fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_,paths_);
    }

    int
    operator()(const Branches::CPtr &branches_,
               const std::string    &fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_.c_str(),paths_);
    }

  private:
    CreateImpl *impl;
  };

  class SearchImpl
  {
  public:
    SearchImpl(const std::string &name_)
      : name(name_)
    {
    }

  public:
    std::string name;
    virtual int operator()(const Branches::CPtr&,const char*,StrVec*) const = 0;
  };

  class Search
  {
  public:
    Search(SearchImpl *impl_)
      : impl(impl_)
    {}

    Search&
    operator=(SearchImpl *impl_)
    {
      impl = impl_;
      return *this;
    }

    const
    std::string&
    name(void) const
    {
      return impl->name;
    }

    int
    operator()(const Branches::CPtr &branches_,
               const char           *fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_,paths_);
    }

    int
    operator()(const Branches::CPtr &branches_,
               const std::string    &fusepath_,
               StrVec               *paths_) const
    {
      return (*impl)(branches_,fusepath_.c_str(),paths_);
    }

  private:
    SearchImpl *impl;
  };
}

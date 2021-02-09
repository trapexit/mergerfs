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

#include "tofrom_string.hpp"
#include "funcs.hpp"
#include "func.hpp"

#include <string>

namespace Category
{
  class Base : public ToFromString
  {
  public:
    int from_string(const std::string &s) final;
    std::string to_string() const final;

  protected:
    std::vector<ToFromString*> funcs;
  };

  class Action final : public Base
  {
  private:
    Action();

  public:
    Action(Funcs &funcs_)
    {
      funcs.push_back(&funcs_.chmod);
      funcs.push_back(&funcs_.chown);
      funcs.push_back(&funcs_.link);
      funcs.push_back(&funcs_.removexattr);
      funcs.push_back(&funcs_.rename);
      funcs.push_back(&funcs_.rmdir);
      funcs.push_back(&funcs_.setxattr);
      funcs.push_back(&funcs_.truncate);
      funcs.push_back(&funcs_.unlink);
      funcs.push_back(&funcs_.utimens);
    }
  };

  class Create final : public Base
  {
  private:
    Create();

  public:
    Create(Funcs &funcs_)
    {
      funcs.push_back(&funcs_.create);
      funcs.push_back(&funcs_.mkdir);
      funcs.push_back(&funcs_.mknod);
      funcs.push_back(&funcs_.symlink);
    }
  };

  class Search final : public Base
  {
  private:
    Search();

  public:
    Search(Funcs &funcs_)
    {
      funcs.push_back(&funcs_.access);
      funcs.push_back(&funcs_.getattr);
      funcs.push_back(&funcs_.getxattr);
      funcs.push_back(&funcs_.listxattr);
      funcs.push_back(&funcs_.open);
      funcs.push_back(&funcs_.readlink);
    }
  };
}

class Categories final
{
private:
  Categories();

public:
  Categories(Funcs &funcs_)
    : action(funcs_),
      create(funcs_),
      search(funcs_)
  {}

public:
  Category::Action action;
  Category::Create create;
  Category::Search search;
};

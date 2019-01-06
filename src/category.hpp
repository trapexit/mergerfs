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

#pragma once

#include <string>
#include <vector>

class Category
{
public:
  struct Enum
  {
    enum Type
      {
        invalid = -1,
        BEGIN   = 0,
        action  = BEGIN,
        create,
        search,
        END
      };
  };

private:
  Enum::Type  _enum;
  std::string _str;

public:
  Category()
    : _enum(invalid),
      _str(invalid)
  {
  }

  Category(const Enum::Type   enum_,
           const std::string &str_)
    : _enum(enum_),
      _str(str_)
  {
  }

public:
  operator const Enum::Type() const { return _enum; }
  operator const std::string&() const { return _str; }
  operator const Category*() const { return this; }

  bool operator==(const std::string &str_) const
  { return _str == str_; }

  bool operator==(const Enum::Type enum_) const
  { return _enum == enum_; }

  bool operator!=(const Category &r) const
  { return _enum != r._enum; }

  bool operator<(const Category &r) const
  { return _enum < r._enum; }

public:
  static const Category &find(const std::string&);
  static const Category &find(const Enum::Type);

public:
  static const std::vector<Category>  _categories_;
  static const Category * const       categories;
  static const Category              &invalid;
  static const Category              &action;
  static const Category              &create;
  static const Category              &search;
};

/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef __CATEGORY_HPP__
#define __CATEGORY_HPP__

#include <string>
#include <vector>

namespace mergerfs
{
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
}

#endif

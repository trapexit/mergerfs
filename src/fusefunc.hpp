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

#ifndef __FUSEFUNC_HPP__
#define __FUSEFUNC_HPP__

#include <string>
#include <vector>

#include "category.hpp"

namespace mergerfs
{
  class FuseFunc
  {
  public:
    struct Enum
    {
      enum Type
        {
          invalid = -1,
          BEGIN   = 0,
          access  = BEGIN,
          chmod,
          chown,
          create,
          getattr,
          getxattr,
          link,
          listxattr,
          mkdir,
          mknod,
          open,
          readlink,
          removexattr,
          rename,
          rmdir,
          setxattr,
          symlink,
          truncate,
          unlink,
          utimens,
          END
        };
    };

  private:
    Enum::Type           _enum;
    std::string          _str;
    Category::Enum::Type _category;

  public:
    FuseFunc()
      : _enum(invalid),
        _str(invalid),
        _category(Category::Enum::invalid)
    {
    }

    FuseFunc(const Enum::Type            enum_,
             const std::string          &str_,
             const Category::Enum::Type  category_)
      : _enum(enum_),
        _str(str_),
        _category(category_)
    {
    }

  public:
    operator const Enum::Type() const { return _enum; }
    operator const std::string&() const { return _str; }
    operator const Category::Enum::Type() const { return _category; }
    operator const FuseFunc*() const { return this; }

    bool operator==(const std::string &str_) const
    { return _str == str_; }

    bool operator==(const Enum::Type enum_) const
    { return _enum == enum_; }

    bool operator!=(const FuseFunc &r) const
    { return _enum != r._enum; }

    bool operator<(const FuseFunc &r) const
    { return _enum < r._enum; }

  public:
    static const FuseFunc &find(const std::string&);
    static const FuseFunc &find(const Enum::Type);

  public:
    static const std::vector<FuseFunc>  _fusefuncs_;
    static const FuseFunc * const       fusefuncs;
    static const FuseFunc              &invalid;
    static const FuseFunc              &access;
    static const FuseFunc              &chmod;
    static const FuseFunc              &chown;
    static const FuseFunc              &create;
    static const FuseFunc              &getattr;
    static const FuseFunc              &getxattr;
    static const FuseFunc              &link;
    static const FuseFunc              &listxattr;
    static const FuseFunc              &mkdir;
    static const FuseFunc              &mknod;
    static const FuseFunc              &open;
    static const FuseFunc              &readlink;
    static const FuseFunc              &removexattr;
    static const FuseFunc              &rename;
    static const FuseFunc              &rmdir;
    static const FuseFunc              &setxattr;
    static const FuseFunc              &symlink;
    static const FuseFunc              &truncate;
    static const FuseFunc              &unlink;
    static const FuseFunc              &utimens;
  };
}

#endif

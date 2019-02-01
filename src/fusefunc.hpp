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

#include "category.hpp"

#include <string>
#include <vector>

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

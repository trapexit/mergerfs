/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_listxattr_func.hpp"
#include "fuse_listxattr_func_factory.hpp"


FUSE::LISTXATTR::Func::Func(const toml::value &toml_)
{
  _listxattr = FuncFactory(toml_);
}

void
FUSE::LISTXATTR::Func::operator=(const toml::value &toml_)
{
  _listxattr = FuncFactory(toml_);
}

int
FUSE::LISTXATTR::Func::operator()(const char   *fusepath_,
                                  char         *list_,
                                  const size_t  size_)
{
  return (*_listxattr)(fusepath_,list_,size_);
}

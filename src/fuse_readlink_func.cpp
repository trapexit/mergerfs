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

#include "fuse_readlink_func.hpp"
#include "fuse_readlink_func_factory.hpp"

#include "state.hpp"


FUSE::READLINK::Func::Func(const toml::value &toml_)
{
  _readlink = FuncFactory(toml_);
}

void
FUSE::READLINK::Func::operator=(const toml::value &toml_)
{
  _readlink = FuncFactory(toml_);
}

int
FUSE::READLINK::Func::operator()(const char   *fusepath_,
                                 char         *buf_,
                                 const size_t  bufsiz_)
{
  return (*_readlink)(fusepath_,buf_,bufsiz_);
}

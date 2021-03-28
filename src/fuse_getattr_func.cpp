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

#include "fuse_getattr_func.hpp"
#include "fuse_getattr_func_factory.hpp"

#include "state.hpp"


FUSE::GETATTR::Func::Func(const toml::value &toml_)
{
  _getattr = FuncFactory(toml_);
}

void
FUSE::GETATTR::Func::operator=(const toml::value &toml_)
{
  _getattr = FuncFactory(toml_);
}

int
FUSE::GETATTR::Func::operator()(const char      *fusepath_,
                                struct stat     *st_,
                                fuse_timeouts_t *timeout_)
{
  return (*_getattr)(fusepath_,st_,timeout_);
}

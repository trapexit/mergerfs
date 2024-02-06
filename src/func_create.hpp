/*
  ISC License

  Copyright (c) 2024, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "func_create_base.hpp"

#include <memory>


namespace Func2
{
  class Create
  {
  public:
    Create&
    operator=(std::unique_ptr<Func2::CreateBase> &&ptr_)
    {
      _func = std::move(ptr_);

      return *this;
    }

  public:
    int
    operator()(Branches2        &branches_,
               fs::Path const   &fusepath_,
               mode_t const      mode_,
               fuse_file_info_t *ffi_)
    {
      return (*_func)(branches_,fusepath_,mode_,ffi_);
    }

  private:
    std::unique_ptr<Func2::CreateBase> _func;
  };
}

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

#pragma once

#include "ghc/filesystem.hpp"
#include "toml.hpp"

#include <sys/stat.h>

#include <functional>


//typedef int (*StatFunc)(const ghc::filesystem::path&,struct stat*);
//typedef std::function<int(const ghc::filesystem::path&,struct stat*)> StatFunc;

namespace Stat
{
  typedef std::function<int(const ghc::filesystem::path&,struct stat*)> Func;

  int no_follow(const ghc::filesystem::path &fullpath,struct stat *st);
  int follow_all(const ghc::filesystem::path &fullpath,struct stat *st);
  int follow_dir(const ghc::filesystem::path &fullpath,struct stat *st);
  int follow_reg(const ghc::filesystem::path &fullpath,struct stat *st);

  Func factory(const std::string&);
}

namespace toml
{
  template<>
  struct from<Stat::Func>
  {
    static
    Stat::Func
    from_toml(const toml::value &v_)
    {
      Stat::Func func;

      func = Stat::factory(v_.as_string());

      return func;
    }
  };
}

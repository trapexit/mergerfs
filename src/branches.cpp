/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "branches.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "fs_glob.hpp"
#include "fs_realpathize.hpp"
#include "num.hpp"
#include "str.hpp"

#include <fnmatch.h>
#include <iostream>


Branches::Branches(const toml::value &toml_)
{
  toml::value branches;
  Branch::Mode default_mode;
  uint64_t default_minfreespace;

  default_mode         = toml::find_or(toml_,"branches","mode",Branch2::Mode::RW);
  default_minfreespace = toml::find_or(toml_,"branches","min-free-space",0);

  branches = toml::find(toml_,"branches");

  for(const auto &branch_group : branches.at("group").as_array())
    {
      if(!toml::find_or(branch_group,"active",true))
        continue;

      emplace_back(branch_group,default_mode,default_minfreespace);
    }
}

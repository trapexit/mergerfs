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

#include "branch_group.hpp"

#include "fs_glob.hpp"
#include "fs_path.hpp"

#include <iostream>
#include <stdexcept>


namespace l
{
  static
  void
  add_literal(const toml::value  &branch_,
              const Branch::Mode  default_mode_,
              const uint64_t      default_minfreespace_,
              BranchGroup        *branch_group_)
  {
    branch_group_->emplace_back(branch_,default_mode_,default_minfreespace_);
  }

  static
  void
  add_glob(const toml::value  &branch_,
           const Branch::Mode  default_mode_,
           const uint64_t      default_minfreespace_,
           BranchGroup        *branch_group_)
  {
    std::string pattern;
    std::vector<gfs::path> paths;

    pattern = toml::find<std::string>(branch_,"path");

    fs::glob(pattern,&paths);

    for(const auto &path : paths)
      {
        toml::value v = branch_;

        v["path"]      = path.native();
        v["path-type"] = "literal";

        l::add_literal(v,default_mode_,default_minfreespace_,branch_group_);
      }
  }
}


BranchGroup::BranchGroup(const toml::value  &toml_,
                         const Branch::Mode  default_mode_,
                         const uint64_t      default_minfreespace_)
{
  Branch::Mode default_mode;
  uint64_t default_minfreespace;

  default_mode         = toml::find_or(toml_,"mode",default_mode_);
  default_minfreespace = toml::find_or(toml_,"min-free-space",default_minfreespace_);

  for(const auto &branch : toml_.at("branch").as_array())
    {
      std::string path_type;

      if(!toml::find_or(branch,"active",true))
        continue;

      path_type = toml::find_or(branch,"path-type","literal");
      if(path_type == "literal")
        l::add_literal(branch,default_mode,default_minfreespace_,this);
      else if(path_type == "glob")
        l::add_glob(branch,default_mode,default_minfreespace,this);
      else
        throw std::runtime_error("invalid path-type = " + path_type);
    }
}

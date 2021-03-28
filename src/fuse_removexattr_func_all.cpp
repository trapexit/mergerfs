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

#include "fuse_removexattr_func_all.hpp"
#include "fuse_removexattr_err.hpp"

#include "fs_lremovexattr.hpp"


namespace gfs = ghc::filesystem;


FUSE::REMOVEXATTR::FuncALL::FuncALL(const toml::value &toml_)
  : _branches(toml_)
{

}

int
FUSE::REMOVEXATTR::FuncALL::operator()(const char *fusepath_,
                                       const char *attrname_)
{
  Err rv;
  gfs::path fusepath;
  gfs::path fullpath;

  fusepath = &fusepath_[1];
  for(const auto &branch_group : _branches)
    {
      for(const auto &branch : branch_group)
        {
          fullpath = branch.path / fusepath;

          rv = fs::lremovexattr(fullpath,attrname_);
        }
    }

  return rv;
}

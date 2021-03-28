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

#include "fuse_link_func_all.hpp"
#include "fuse_link_err.hpp"

#include "fs_clonepath_branches.hpp"
#include "fs_exists.hpp"
#include "fs_link.hpp"


namespace gfs = ghc::filesystem;


FUSE::LINK::FuncALL::FuncALL(const toml::value &toml_)
  : _branches(toml_)
{

}

int
FUSE::LINK::FuncALL::operator()(const char *oldpath_,
                                const char *newpath_)
{
  int rv;
  Err err;
  gfs::path oldpath;
  gfs::path newpath;
  gfs::path fulloldpath;
  gfs::path fullnewpath;

  oldpath = &oldpath_[1];
  newpath = &newpath_[1];
  for(const auto &branch_group : _branches)
    {
      for(const auto &branch : branch_group)
        {
          fulloldpath = branch.path / oldpath;
          fullnewpath = branch.path / newpath;;

          rv = fs::link(fulloldpath,fullnewpath);
          if(rv == -ENOENT)
            {
              if(!fs::exists(fulloldpath))
                continue;

              rv = fs::clonepath_as_root(_branches,branch.path,newpath.parent_path());
              if(rv < 0)
                continue;

              rv = fs::link(fulloldpath,fullnewpath);
            }

          err = rv;
        }
    }

  return err;
}

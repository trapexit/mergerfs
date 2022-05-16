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

#include "fuse_getxattr_policy_ff.hpp"

#include "fs_lgetxattr.hpp"


FUSE::GETXATTR::POLICY::FF::FF(const toml::value &toml_)
  : _branches(toml_)
{

}

int
FUSE::GETXATTR::POLICY::FF::operator()(const gfs::path &fusepath_,
                                       const char      *attrname_,
                                       char            *buf_,
                                       size_t           count_)
{
  int rv;
  gfs::path fullpath;

  for(const auto &branch_group : _branches)
    {
      for(const auto &branch : branch_group)
        {
          fullpath  = branch.path / fusepath_;

          rv = fs::lgetxattr(fullpath,attrname_,buf_,count_);
          if(rv == -ENOENT)
            continue;

          return rv;
        }
    }

  return -ENOENT;
}

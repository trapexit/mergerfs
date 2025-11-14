/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_access.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_eaccess.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include <string>
#include <vector>


static
int
_access(const Policy::Search &searchFunc_,
        const Branches       &branches_,
        const fs::path       &fusepath_,
        const int             mask_)
{
  int rv;
  StrVec basepaths;
  fs::path fullpath;
  std::vector<Branch*> branches;

  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  fullpath = branches[0]->path / fusepath_;

  rv = fs::eaccess(fullpath,mask_);

  return rv;
}

int
FUSE::access(const fuse_req_ctx_t *ctx_,
             const char           *fusepath_,
             int                   mask_)
{
  const fs::path fusepath{fusepath_};

  return cfg.access(cfg.branches,
                    fusepath,
                    mask_);
}

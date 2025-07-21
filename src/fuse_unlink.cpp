/*
  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_unlink.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_path.hpp"
#include "fs_unlink.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

#include <unistd.h>


static
int
_unlink_loop(const std::vector<Branch*> &branches_,
             const char                 *fusepath_)
{
  Err err;
  std::string fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = fs::path::make(branch->path,fusepath_);

      err = fs::unlink(fullpath);
    }

  return err;
}

static
int
_unlink(const Policy::Action &unlinkPolicy_,
        const Branches       &branches_,
        const char           *fusepath_)
{
  int rv;
  std::vector<Branch*> branches;

  rv = unlinkPolicy_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  return ::_unlink_loop(branches,fusepath_);
}

int
FUSE::unlink(const char *fusepath_)
{
  Config::Read cfg;
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_unlink(cfg->func.unlink.policy,
                   cfg->branches,
                   fusepath_);
}

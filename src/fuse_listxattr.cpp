/*
  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_listxattr.hpp"

#include "config.hpp"
#include "xattr.hpp"


int
FUSE::listxattr(const fuse_req_ctx_t *ctx_,
                const char           *fusepath_,
                char                 *list_,
                const size_t          size_)
{
  ssize_t rv;
  const fs::path fusepath{fusepath_};

  if(Config::is_ctrl_file(fusepath))
    return cfg.keys_listxattr(list_,size_);

  switch(cfg.xattr)
    {
    case XAttr::ENUM::PASSTHROUGH:
      break;
    case XAttr::ENUM::NOATTR:
      return 0;
    case XAttr::ENUM::NOSYS:
      return -ENOSYS;
    }

  rv = cfg.listxattr(cfg.branches,
                     fusepath,
                     list_,
                     size_);

  return rv;
}

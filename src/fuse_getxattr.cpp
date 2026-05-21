/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_getxattr.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "str.hpp"

#include "fuse.h"

#include <cstring>
#include <string>


static const char SECURITY_CAPABILITY[] = "security.capability";


static
bool
_is_attrname_security_capability(const char *attrname_)
{
  return str::eq(attrname_,SECURITY_CAPABILITY);
}

static
int
_getxattr_ctrl_file(Config       &cfg_,
                    const char   *attrname_,
                    char         *buf_,
                    const size_t  count_)
{
  int rv;
  std::string key;
  std::string val;

  if(!Config::is_mergerfs_xattr(attrname_))
    return -ENOATTR;

  key = Config::prune_ctrl_xattr(attrname_);
  rv = cfg_.get(key,&val);
  if(rv < 0)
    return rv;

  if(count_ == 0)
    return val.size();

  if(count_ < val.size())
    return -ERANGE;

  memcpy(buf_,val.c_str(),val.size());

  return (int)val.size();
}

int
FUSE::getxattr(const fuse_req_ctx_t *ctx_,
               const char           *fusepath_,
               const char           *attrname_,
               char                 *attrval_,
               size_t                attrvalsize_)
{
  const fs::path fusepath{fusepath_};

  if(Config::is_ctrl_file(fusepath))
    return ::_getxattr_ctrl_file(cfg,
                                 attrname_,
                                 attrval_,
                                 attrvalsize_);

  if((cfg.security_capability == false) &&
     ::_is_attrname_security_capability(attrname_))
    return -ENOATTR;

  if(cfg.xattr.to_int())
    return -cfg.xattr.to_int();

  return cfg.getxattr(cfg.branches,
                      fusepath,
                      attrname_,
                      attrval_,
                      attrvalsize_);
}

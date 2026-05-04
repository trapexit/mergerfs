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

#pragma once

#include "tofrom_string.hpp"
#include "from_string.hpp"
#include "to_string.hpp"

#include "fuse_cfg.hpp"

#include <errno.h>


class CfgRememberNodes : public ToFromString
{
public:
  int
  from_string(const std::string_view s_)
  {
    int rv;
    s64 v;

    rv = str::from(s_,&v);
    if(rv < 0)
      return rv;
    if(v < -1)
      return -EINVAL;

    fuse_cfg.remember_nodes.store(v,std::memory_order_relaxed);

    return 0;
  }

  std::string
  to_string() const
  {
    return str::to(fuse_cfg.remember_nodes.load(std::memory_order_relaxed));
  }
};


class CfgNoforget : public ToFromString
{
public:
  int
  from_string(const std::string_view s_)
  {
    int rv;
    bool b = false;

    rv = str::from(s_,&b);
    if((b == true) || s_.empty())
      {
        fuse_cfg.remember_nodes.store(-1,std::memory_order_relaxed);
        return 0;
      }

    if(rv)
      return rv;

    fuse_cfg.remember_nodes.store(0,std::memory_order_relaxed);

    return 0;
  }

  std::string
  to_string() const
  {
    if(fuse_cfg.remember_nodes.load(std::memory_order_relaxed) == -1)
      return "true";
    return "false";
  }
};

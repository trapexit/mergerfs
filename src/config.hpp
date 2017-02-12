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

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <fuse.h>

#include <sys/stat.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "policy.hpp"
#include "fusefunc.hpp"

namespace mergerfs
{
  class Config
  {
  public:
    Config();

  public:
    int set_func_policy(const std::string &fusefunc_,
                        const std::string &policy_);
    int set_category_policy(const std::string &category,
                            const std::string &policy);

  public:
    std::string              destmount;
    std::vector<std::string> srcmounts;
    mutable pthread_rwlock_t srcmountslock;
    uint64_t                 minfreespace;
    bool                     moveonenospc;
    bool                     direct_io;
    bool                     cifs;

  public:
    const Policy  *policies[FuseFunc::Enum::END];
    const Policy *&access;
    const Policy *&chmod;
    const Policy *&chown;
    const Policy *&create;
    const Policy *&getattr;
    const Policy *&getxattr;
    const Policy *&link;
    const Policy *&listxattr;
    const Policy *&mkdir;
    const Policy *&mknod;
    const Policy *&open;
    const Policy *&readlink;
    const Policy *&removexattr;
    const Policy *&rename;
    const Policy *&rmdir;
    const Policy *&setxattr;
    const Policy *&symlink;
    const Policy *&truncate;
    const Policy *&unlink;
    const Policy *&utimens;

  public:
    const std::string controlfile;

  public:
    static
    const Config &
    get(const fuse_context *fc)
    {
      return *((Config*)fc->private_data);
    }

    static
    Config &
    get_writable(void)
    {
      return (*((Config*)fuse_get_context()->private_data));
    }
  };
}

#endif

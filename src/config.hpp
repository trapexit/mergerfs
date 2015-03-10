/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <fuse.h>

#include <sys/stat.h>

#include <string>
#include <vector>

#include "policy.hpp"
#include "fusefunc.hpp"

namespace mergerfs
{
  namespace config
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
    };

    const Config &get(const struct fuse_context *fc);
    const Config &get(void);
    Config       &get_writable(void);
  }
}

#endif

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

#include <string>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>

#include "config.hpp"
#include "rwlock.hpp"
#include "fs.hpp"

#define POLICYINIT(X) X(policies[FuseFunc::Enum::X])

using std::string;
using std::vector;

namespace mergerfs
{
  namespace config
  {
    Config::Config()
      : destmount(),
        srcmounts(),
        srcmountslock(),
        POLICYINIT(access),
        POLICYINIT(chmod),
        POLICYINIT(chown),
        POLICYINIT(create),
        POLICYINIT(getattr),
        POLICYINIT(getxattr),
        POLICYINIT(link),
        POLICYINIT(listxattr),
        POLICYINIT(mkdir),
        POLICYINIT(mknod),
        POLICYINIT(open),
        POLICYINIT(readlink),
        POLICYINIT(removexattr),
        POLICYINIT(rename),
        POLICYINIT(rmdir),
        POLICYINIT(setxattr),
        POLICYINIT(symlink),
        POLICYINIT(truncate),
        POLICYINIT(unlink),
        POLICYINIT(utimens),
        controlfile("/.mergerfs")
    {
      pthread_rwlock_init(&srcmountslock,NULL);

      setpolicy(Category::Enum::action,Policy::Enum::ff);
      setpolicy(Category::Enum::create,Policy::Enum::epmfs);
      setpolicy(Category::Enum::search,Policy::Enum::ff);
    }

    void
    Config::setpolicy(const Category::Enum::Type category,
                      const Policy::Enum::Type   policy_)
    {
      const Policy *policy = Policy::find(policy_);

      for(int i = 0; i < FuseFunc::Enum::END; i++)
        {
          if(FuseFunc::fusefuncs[i] == category)
            policies[(FuseFunc::Enum::Type)FuseFunc::fusefuncs[i]] = policy;
        }
    }

    const Config&
    get(const struct fuse_context *fc)
    {
      return *((Config*)fc->private_data);
    }

    const Config&
    get(void)
    {
      return get(fuse_get_context());
    }

    Config&
    get_writable(void)
    {
      return (*((Config*)fuse_get_context()->private_data));
    }
  }
}

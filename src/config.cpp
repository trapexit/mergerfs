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
#include <errno.h>

#include "config.hpp"
#include "rwlock.hpp"
#include "fs.hpp"

#define POLICYINIT(X) X(policies[FuseFunc::Enum::X])

using std::string;
using std::vector;

namespace mergerfs
{
  Config::Config()
    : destmount(),
      srcmounts(),
      srcmountslock(),
      minfreespace(UINT32_MAX),
      moveonenospc(false),
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

    set_category_policy("action","all");
    set_category_policy("create","epmfs");
    set_category_policy("search","ff");
  }

  int
  Config::set_func_policy(const string &fusefunc_,
                          const string &policy_)
  {
    const Policy   *policy;
    const FuseFunc *fusefunc;

    fusefunc = FuseFunc::find(fusefunc_);
    if(fusefunc == FuseFunc::invalid)
      return (errno=ENODATA,-1);

    policy = Policy::find(policy_);
    if(policy == Policy::invalid)
      return (errno=EINVAL,-1);

    policies[(FuseFunc::Enum::Type)*fusefunc] = policy;

    return 0;
  }

  int
  Config::set_category_policy(const string &category_,
                              const string &policy_)
  {
    const Policy   *policy;
    const Category *category;

    category = Category::find(category_);
    if(category == Category::invalid)
      return (errno=ENODATA,-1);

    policy = Policy::find(policy_);
    if(policy == Policy::invalid)
      return (errno=EINVAL,-1);

    for(int i = 0; i < FuseFunc::Enum::END; i++)
      {
        if(FuseFunc::fusefuncs[i] == (Category::Enum::Type)*category)
          policies[(FuseFunc::Enum::Type)FuseFunc::fusefuncs[i]] = policy;
      }

    return 0;
  }
}

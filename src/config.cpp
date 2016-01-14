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

#include <string>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.hpp"
#include "rwlock.hpp"
#include "fs.hpp"

#define MINFREESPACE_DEFAULT (4294967295ULL)
#define POLICYINIT(X) X(policies[FuseFunc::Enum::X])

using std::string;
using std::vector;

namespace mergerfs
{
  Config::Config()
    : destmount(),
      srcmounts(),
      srcmountslock(),
      minfreespace(MINFREESPACE_DEFAULT),
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

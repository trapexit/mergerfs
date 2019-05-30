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

#include "config.hpp"
#include "errno.hpp"
#include "fs.hpp"
#include "rwlock.hpp"

#include <string>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>

#define MINFREESPACE_DEFAULT (4294967295ULL)
#define POLICYINIT(X) X(policies[FuseFunc::Enum::X])

using std::string;
using std::vector;

Config::Config()
  : destmount(),
    branches(),
    branches_lock(),
    minfreespace(MINFREESPACE_DEFAULT),
    moveonenospc(false),
    direct_io(false),
    dropcacheonclose(false),
    symlinkify(false),
    symlinkify_timeout(3600),
    nullrw(false),
    ignorepponrename(false),
    security_capability(true),
    link_cow(false),
    xattr(0),
    statfs(StatFS::BASE),
    statfs_ignore(StatFSIgnore::NONE),
    posix_acl(false),
    cache_symlinks(false),
    cache_readdir(false),
    async_read(true),
    cache_files(CacheFiles::LIBFUSE),
    fuse_msg_size(FUSE_MAX_MAX_PAGES),
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
  pthread_rwlock_init(&branches_lock,NULL);

  set_category_policy("action","epall");
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

Config::CacheFiles::operator int() const
{
  return _data;
}

Config::CacheFiles::operator std::string() const
{
  switch(_data)
    {
    case OFF:
      return "off";
    case PARTIAL:
      return "partial";
    case FULL:
      return "full";
    case AUTO_FULL:
      return "auto-full";
    case LIBFUSE:
      return "libfuse";
    case INVALID:
      break;
    }

  return "";
}

Config::CacheFiles::CacheFiles()
  : _data(INVALID)
{

}

Config::CacheFiles::CacheFiles(Config::CacheFiles::Enum data_)
  : _data(data_)
{

}

bool
Config::CacheFiles::valid() const
{
  return (_data != INVALID);
}

Config::CacheFiles&
Config::CacheFiles::operator=(const Config::CacheFiles::Enum data_)
{
  _data = data_;

  return *this;
}

Config::CacheFiles&
Config::CacheFiles::operator=(const std::string &data_)
{
  if(data_ == "off")
    _data = OFF;
  else if(data_ == "partial")
    _data = PARTIAL;
  else if(data_ == "full")
    _data = FULL;
  else if(data_ == "auto-full")
    _data = AUTO_FULL;
  else if(data_ == "libfuse")
    _data = LIBFUSE;
  else
    _data = INVALID;

  return *this;
}

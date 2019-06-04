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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_getxattr.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "rwlock.hpp"
#include "str.hpp"
#include "ugid.hpp"
#include "version.hpp"

#include <fuse.h>

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

static const char SECURITY_CAPABILITY[] = "security.capability";

using std::string;
using std::vector;
using std::set;

namespace l
{
  static
  bool
  is_attrname_security_capability(const char *attrname_)
  {
    return (strcmp(attrname_,SECURITY_CAPABILITY) == 0);
  }

  static
  int
  lgetxattr(const string &path_,
            const char   *attrname_,
            void         *value_,
            const size_t  size_)
  {
    int rv;

    rv = fs::lgetxattr(path_,attrname_,value_,size_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  void
  getxattr_controlfile_fusefunc_policy(const Config &config_,
                                       const string &attr_,
                                       string       &attrvalue_)
  {
    FuseFunc fusefunc;

    fusefunc = FuseFunc::find(attr_);
    if(fusefunc != FuseFunc::invalid)
      attrvalue_ = (std::string)*config_.policies[(FuseFunc::Enum::Type)*fusefunc];
  }

  static
  void
  getxattr_controlfile_category_policy(const Config &config_,
                                       const string &attr_,
                                       string       &attrvalue_)
  {
    Category cat;

    cat = Category::find(attr_);
    if(cat != Category::invalid)
      {
        vector<string> policies;
        for(int i = FuseFunc::Enum::BEGIN; i < FuseFunc::Enum::END; i++)
          {
            if(cat == (Category::Enum::Type)*FuseFunc::fusefuncs[i])
              policies.push_back(*config_.policies[i]);
          }

        std::sort(policies.begin(),policies.end());
        policies.erase(std::unique(policies.begin(),policies.end()),
                       policies.end());
        attrvalue_ = str::join(policies,',');
      }
  }

  static
  void
  getxattr_controlfile_srcmounts(const Config &config_,
                                 string       &attrvalue_)
  {
    attrvalue_ = config_.branches.to_string();
  }

  static
  void
  getxattr_controlfile_branches(const Config &config_,
                                string       &attrvalue_)
  {
    attrvalue_ = config_.branches.to_string(true);
  }

  static
  void
  getxattr_controlfile_uint64_t(const uint64_t  uint_,
                                string         &attrvalue_)
  {
    std::ostringstream os;

    os << uint_;

    attrvalue_ = os.str();
  }

  static
  void
  getxattr_controlfile_double(const double  d_,
                              string       &attrvalue_)
  {
    std::ostringstream os;

    os << d_;

    attrvalue_ = os.str();
  }

  static
  void
  getxattr_controlfile_time_t(const time_t  time,
                              string       &attrvalue)
  {
    std::ostringstream os;

    os << time;

    attrvalue = os.str();
  }

  static
  void
  getxattr_controlfile_bool(const bool  boolvalue,
                            string     &attrvalue)
  {
    attrvalue = (boolvalue ? "true" : "false");
  }

  static
  void
  getxattr_controlfile_errno(const int  errno_,
                             string    &attrvalue)
  {
    switch(errno_)
      {
      case 0:
        attrvalue = "passthrough";
        break;
      case ENOATTR:
        attrvalue = "noattr";
        break;
      case ENOSYS:
        attrvalue = "nosys";
        break;
      default:
        attrvalue = "ERROR";
        break;
      }
  }

  static
  void
  getxattr_controlfile_statfs(const Config::StatFS::Enum  enum_,
                              string                     &attrvalue_)
  {
    switch(enum_)
      {
      case Config::StatFS::BASE:
        attrvalue_ = "base";
        break;
      case Config::StatFS::FULL:
        attrvalue_ = "full";
        break;
      default:
        attrvalue_ = "ERROR";
        break;
      }
  }

  static
  void
  getxattr_controlfile_statfsignore(const Config::StatFSIgnore::Enum  enum_,
                                    string                           &attrvalue_)
  {
    switch(enum_)
      {
      case Config::StatFSIgnore::NONE:
        attrvalue_ = "none";
        break;
      case Config::StatFSIgnore::RO:
        attrvalue_ = "ro";
        break;
      case Config::StatFSIgnore::NC:
        attrvalue_ = "nc";
        break;
      default:
        attrvalue_ = "ERROR";
        break;
      }
  }

  static
  void
  getxattr_controlfile(const Config::CacheFiles &cache_files_,
                       string                   &attrvalue_)
  {
    attrvalue_ = (string)cache_files_;
  }

  static
  void
  getxattr_controlfile(const uint16_t &uint16_,
                       string         &attrvalue_)
  {
    std::ostringstream os;

    os << uint16_;

    attrvalue_ = os.str();
  }

  static
  void
  getxattr_controlfile_policies(const Config &config,
                                string       &attrvalue)
  {
    size_t i = Policy::Enum::begin();

    attrvalue = (string)Policy::policies[i];
    for(i++; i < Policy::Enum::end(); i++)
      attrvalue += ',' + (string)Policy::policies[i];
  }

  static
  void
  getxattr_controlfile_version(string &attrvalue)
  {
    attrvalue = MERGERFS_VERSION;
    if(attrvalue.empty())
      attrvalue = "unknown_possible_problem_with_build";
  }

  static
  void
  getxattr_controlfile_pid(string &attrvalue)
  {
    int pid;
    char buf[32];

    pid = getpid();
    snprintf(buf,sizeof(buf),"%d",pid);

    attrvalue = buf;
  }

  static
  void
  getxattr_controlfile_cache_attr(string &attrvalue)
  {
    double d;

    d = fuse_config_get_attr_timeout(fuse_get_context()->fuse);

    l::getxattr_controlfile_double(d,attrvalue);
  }

  static
  void
  getxattr_controlfile_cache_entry(string &attrvalue)
  {
    double d;

    d = fuse_config_get_entry_timeout(fuse_get_context()->fuse);

    l::getxattr_controlfile_double(d,attrvalue);
  }

  static
  void
  getxattr_controlfile_cache_negative_entry(string &attrvalue)
  {
    double d;

    d = fuse_config_get_negative_entry_timeout(fuse_get_context()->fuse);

    l::getxattr_controlfile_double(d,attrvalue);
  }

  static
  int
  getxattr_controlfile(const Config &config,
                       const char   *attrname,
                       char         *buf,
                       const size_t  count)
  {
    size_t len;
    string attrvalue;
    vector<string> attr;

    str::split(attr,attrname,'.');
    if((attr[0] != "user") || (attr[1] != "mergerfs"))
      return -ENOATTR;

    switch(attr.size())
      {
      case 3:
        if(attr[2] == "srcmounts")
          l::getxattr_controlfile_srcmounts(config,attrvalue);
        else if(attr[2] == "branches")
          l::getxattr_controlfile_branches(config,attrvalue);
        else if(attr[2] == "minfreespace")
          l::getxattr_controlfile_uint64_t(config.minfreespace,attrvalue);
        else if(attr[2] == "moveonenospc")
          l::getxattr_controlfile_bool(config.moveonenospc,attrvalue);
        else if(attr[2] == "dropcacheonclose")
          l::getxattr_controlfile_bool(config.dropcacheonclose,attrvalue);
        else if(attr[2] == "symlinkify")
          l::getxattr_controlfile_bool(config.symlinkify,attrvalue);
        else if(attr[2] == "symlinkify_timeout")
          l::getxattr_controlfile_time_t(config.symlinkify_timeout,attrvalue);
        else if(attr[2] == "nullrw")
          l::getxattr_controlfile_bool(config.nullrw,attrvalue);
        else if(attr[2] == "ignorepponrename")
          l::getxattr_controlfile_bool(config.ignorepponrename,attrvalue);
        else if(attr[2] == "security_capability")
          l::getxattr_controlfile_bool(config.security_capability,attrvalue);
        else if(attr[2] == "xattr")
          l::getxattr_controlfile_errno(config.xattr,attrvalue);
        else if(attr[2] == "link_cow")
          l::getxattr_controlfile_bool(config.link_cow,attrvalue);
        else if(attr[2] == "statfs")
          l::getxattr_controlfile_statfs(config.statfs,attrvalue);
        else if(attr[2] == "statfs_ignore")
          l::getxattr_controlfile_statfsignore(config.statfs_ignore,attrvalue);
        else if(attr[2] == "policies")
          l::getxattr_controlfile_policies(config,attrvalue);
        else if(attr[2] == "version")
          l::getxattr_controlfile_version(attrvalue);
        else if(attr[2] == "pid")
          l::getxattr_controlfile_pid(attrvalue);
        else if(attr[2] == "direct_io")
          l::getxattr_controlfile_bool(config.direct_io,attrvalue);
        else if(attr[2] == "posix_acl")
          l::getxattr_controlfile_bool(config.posix_acl,attrvalue);
        else if(attr[2] == "async_read")
          l::getxattr_controlfile_bool(config.async_read,attrvalue);
        else if(attr[2] == "fuse_msg_size")
          l::getxattr_controlfile(config.fuse_msg_size,attrvalue);
        break;

      case 4:
        if(attr[2] == "category")
          l::getxattr_controlfile_category_policy(config,attr[3],attrvalue);
        else if(attr[2] == "func")
          l::getxattr_controlfile_fusefunc_policy(config,attr[3],attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "open"))
          l::getxattr_controlfile_uint64_t(config.open_cache.timeout,attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "statfs"))
          l::getxattr_controlfile_uint64_t(fs::statvfs_cache_timeout(),attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "attr"))
          l::getxattr_controlfile_cache_attr(attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "entry"))
          l::getxattr_controlfile_cache_entry(attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "negative_entry"))
          l::getxattr_controlfile_cache_negative_entry(attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "symlinks"))
          l::getxattr_controlfile_bool(config.cache_symlinks,attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "readdir"))
          l::getxattr_controlfile_bool(config.cache_readdir,attrvalue);
        else if((attr[2] == "cache") && (attr[3] == "files"))
          l::getxattr_controlfile(config.cache_files,attrvalue);
        break;
      }

    if(attrvalue.empty())
      return -ENOATTR;

    len = attrvalue.size();

    if(count == 0)
      return len;

    if(count < len)
      return -ERANGE;

    memcpy(buf,attrvalue.c_str(),len);

    return (int)len;
  }

  static
  int
  getxattr_from_string(char         *destbuf,
                       const size_t  destbufsize,
                       const string &src)
  {
    const size_t srcbufsize = src.size();

    if(destbufsize == 0)
      return srcbufsize;

    if(srcbufsize > destbufsize)
      return -ERANGE;

    memcpy(destbuf,src.data(),srcbufsize);

    return srcbufsize;
  }

  static
  int
  getxattr_user_mergerfs_allpaths(const Branches &branches_,
                                  const char     *fusepath,
                                  char           *buf,
                                  const size_t    count)
  {
    string concated;
    vector<string> paths;
    vector<string> branches;

    branches_.to_paths(branches);

    fs::findallfiles(branches,fusepath,paths);

    concated = str::join(paths,'\0');

    return l::getxattr_from_string(buf,count,concated);
  }

  static
  int
  getxattr_user_mergerfs(const string         &basepath,
                         const char           *fusepath,
                         const string         &fullpath,
                         const Branches       &branches_,
                         const char           *attrname,
                         char                 *buf,
                         const size_t          count)
  {
    vector<string> attr;

    str::split(attr,attrname,'.');

    if(attr[2] == "basepath")
      return l::getxattr_from_string(buf,count,basepath);
    else if(attr[2] ==  "relpath")
      return l::getxattr_from_string(buf,count,fusepath);
    else if(attr[2] == "fullpath")
      return l::getxattr_from_string(buf,count,fullpath);
    else if(attr[2] == "allpaths")
      return l::getxattr_user_mergerfs_allpaths(branches_,fusepath,buf,count);

    return -ENOATTR;
  }

  static
  int
  getxattr(Policy::Func::Search  searchFunc,
           const Branches       &branches_,
           const size_t          minfreespace,
           const char           *fusepath,
           const char           *attrname,
           char                 *buf,
           const size_t          count)
  {
    int rv;
    string fullpath;
    vector<const string*> basepaths;

    rv = searchFunc(branches_,fusepath,minfreespace,basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath);

    if(str::isprefix(attrname,"user.mergerfs."))
      return l::getxattr_user_mergerfs(*basepaths[0],
                                       fusepath,
                                       fullpath,
                                       branches_,
                                       attrname,
                                       buf,
                                       count);

    return l::lgetxattr(fullpath,attrname,buf,count);
  }
}

namespace FUSE
{
  int
  getxattr(const char *fusepath,
           const char *attrname,
           char       *buf,
           size_t      count)
  {
    const fuse_context *fc     = fuse_get_context();
    const Config       &config = Config::get(fc);

    if(fusepath == config.controlfile)
      return l::getxattr_controlfile(config,
                                     attrname,
                                     buf,
                                     count);

    if((config.security_capability == false) &&
       l::is_attrname_security_capability(attrname))
      return -ENOATTR;

    if(config.xattr)
      return -config.xattr;

    const ugid::Set         ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard readlock(&config.branches_lock);

    return l::getxattr(config.getxattr,
                       config.branches,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       buf,
                       count);
  }
}

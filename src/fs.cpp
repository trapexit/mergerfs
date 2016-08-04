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

#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "fs_attr.hpp"
#include "fs_path.hpp"
#include "fs_xattr.hpp"
#include "statvfs_util.hpp"
#include "str.hpp"
#include "success_fail.hpp"

using std::string;
using std::vector;

namespace fs
{
  bool
  exists(const string &path,
         struct stat  &st)
  {
    int rv;

    rv = ::lstat(path.c_str(),&st);

    return LSTAT_SUCCEEDED(rv);
  }

  bool
  exists(const string &path)
  {
    struct stat st;

    return exists(path,st);
  }

  bool
  statvfs(const string   &path,
          struct statvfs &st)
  {
    int rv;

    rv = ::statvfs(path.c_str(),&st);

    return STATVFS_SUCCEEDED(rv);
  }

  bool
  info(const string &path,
       bool         &readonly,
       uint64_t     &spaceavail,
       uint64_t     &spaceused)
  {
    bool rv;
    struct statvfs st;

    rv = fs::statvfs(path,st);
    if(rv)
      {
        readonly   = StatVFS::readonly(st);
        spaceavail = StatVFS::spaceavail(st);
        spaceused  = StatVFS::spaceused(st);
      }

    return rv;
  }

  bool
  readonly(const string &path)
  {
    bool rv;
    struct statvfs st;

    rv = fs::statvfs(path,st);

    return (rv && StatVFS::readonly(st));
  }

  bool
  spaceavail(const string &path,
             uint64_t       &spaceavail)
  {
    bool rv;
    struct statvfs st;

    rv = fs::statvfs(path,st);
    if(rv)
      spaceavail = StatVFS::spaceavail(st);

    return rv;
  }

  bool
  spaceused(const string &path,
            uint64_t     &spaceused)
  {
    bool rv;
    struct statvfs st;

    rv = fs::statvfs(path,st);
    if(rv)
      spaceused = StatVFS::spaceused(st);

    return rv;
  }

  void
  findallfiles(const vector<string> &srcmounts,
               const char           *fusepath,
               vector<string>       &paths)
  {
    string fullpath;

    for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
      {
        fs::path::make(&srcmounts[i],fusepath,fullpath);

        if(!fs::exists(fullpath))
          continue;

        paths.push_back(fullpath);
      }
  }

  int
  findonfs(const vector<string> &srcmounts,
           const char           *fusepath,
           const int             fd,
           string               &basepath)
  {
    int rv;
    dev_t dev;
    string fullpath;
    struct stat st;

    rv = ::fstat(fd,&st);
    if(FSTAT_FAILED(rv))
      return -1;

    dev = st.st_dev;
    for(int i = 0, ei = srcmounts.size(); i != ei; i++)
      {
        fs::path::make(&srcmounts[i],fusepath,fullpath);

        rv = ::lstat(fullpath.c_str(),&st);
        if(FSTAT_FAILED(rv))
          continue;

        if(st.st_dev != dev)
          continue;

        basepath = srcmounts[i];

        return 0;
      }

    return (errno=ENOENT,-1);
  }

  void
  glob(const vector<string> &patterns,
       vector<string>       &strs)
  {
    int flags;
    size_t veclen;
    glob_t gbuf = {0};

    veclen = patterns.size();
    if(veclen == 0)
      return;

    flags = 0;
    glob(patterns[0].c_str(),flags,NULL,&gbuf);

    flags = GLOB_APPEND;
    for(size_t i = 1; i < veclen; i++)
      glob(patterns[i].c_str(),flags,NULL,&gbuf);

    for(size_t i = 0; i < gbuf.gl_pathc; ++i)
      strs.push_back(gbuf.gl_pathv[i]);

    globfree(&gbuf);
  }

  void
  realpathize(vector<string> &strs)
  {
    char *rv;

    for(size_t i = 0; i < strs.size(); i++)
      {
        rv = ::realpath(strs[i].c_str(),NULL);
        if(rv == NULL)
          continue;

        strs[i] = rv;

        ::free(rv);
      }
  }

  int
  getfl(const int fd)
  {
    return ::fcntl(fd,F_GETFL,0);
  }

  int
  mfs(const vector<string> &basepaths,
      const uint64_t        minfreespace,
      string               &path)
  {
    fsblkcnt_t mfs;
    const string *mfsbasepath;

    mfs = 0;
    mfsbasepath = NULL;
    for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
      {
        uint64_t spaceavail;
        const string &basepath = basepaths[i];

        if(!fs::spaceavail(basepath,spaceavail))
          continue;

        if(spaceavail < minfreespace)
          continue;
        if(spaceavail <= mfs)
          continue;

        mfs         = spaceavail;
        mfsbasepath = &basepaths[i];
      }

    if(mfsbasepath == NULL)
      return (errno=ENOENT,-1);

    path = *mfsbasepath;

    return 0;
  }
};

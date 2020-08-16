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

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "errno.hpp"
#include "fs_attr.hpp"
#include "fs_base_realpath.hpp"
#include "fs_base_stat.hpp"
#include "fs_exists.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "fs_xattr.hpp"
#include "str.hpp"

using std::string;
using std::vector;

namespace fs
{
  void
  findallfiles(const vector<string> &basepaths_,
               const char           *fusepath_,
               vector<string>       *paths_)
  {
    string fullpath;

    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        fullpath = fs::path::make(basepaths_[i],fusepath_);

        if(!fs::exists(fullpath))
          continue;

        paths_->push_back(fullpath);
      }
  }

  int
  findonfs(const vector<string> &basepaths,
           const string         &fusepath,
           const int             fd,
           string               &basepath)
  {
    int rv;
    dev_t dev;
    string fullpath;
    struct stat st;

    rv = fs::fstat(fd,&st);
    if(rv == -1)
      return -1;

    dev = st.st_dev;
    for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
      {
        fullpath = fs::path::make(basepaths[i],fusepath);

        rv = fs::lstat(fullpath,&st);
        if(rv == -1)
          continue;

        if(st.st_dev != dev)
          continue;

        basepath = basepaths[i];

        return 0;
      }

    return (errno=ENOENT,-1);
  }

  void
  realpathize(vector<string> *strs_)
  {
    char *rv;

    for(size_t i = 0; i < strs_->size(); i++)
      {
        rv = fs::realpath((*strs_)[i]);
        if(rv == NULL)
          continue;

        (*strs_)[i] = rv;

        ::free(rv);
      }
  }

  int
  getfl(const int fd)
  {
    return ::fcntl(fd,F_GETFL,0);
  }

  int
  setfl(const int    fd,
        const mode_t mode)
  {
    return ::fcntl(fd,F_SETFL,mode);
  }
};

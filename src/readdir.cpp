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

#include <fuse.h>

#include <string>
#include <set>
#include <vector>

#include <errno.h>
#include <dirent.h>

#include "readdir.hpp"
#include "config.hpp"
#include "ugid.hpp"
#include "fs.hpp"

using std::string;
using std::vector;
using std::set;
using std::pair;
using mergerfs::readdir::FileData;

#define NO_OFFSET 0

static
int
_readdir(const vector<string>  &srcmounts,
         const string           dirname,
         void                  *buf,
         const fuse_fill_dir_t  filler)
{
  set<string> found;

  for(vector<string>::const_iterator
        iter = srcmounts.begin(), enditer = srcmounts.end();
      iter != enditer;
      ++iter)
    {
      DIR *dh;
      string basepath;

      basepath = fs::make_path(*iter,dirname);
      dh = ::opendir(basepath.c_str());
      if(!dh)
        continue;

      for(struct dirent *de = ::readdir(dh); de != NULL; de = ::readdir(dh))
        {
          string d_name(de->d_name);

          if(found.insert(d_name).second == false)
            continue;

          if(filler(buf,de->d_name,NULL,NO_OFFSET) != 0)
            return -ENOMEM;
        }

      ::closedir(dh);
    }

  if(found.empty())
    return -ENOENT;

  return 0;
}

static
int
stat_vector_filler(void              *buf,
                   const char        *name,
                   const struct stat *stbuf,
                   off_t              off)
{
  vector<FileData> *stats = (vector<FileData>*)buf;

  stats->push_back(FileData(name,*stbuf));

  return 0;
}

namespace mergerfs
{
  namespace readdir
  {
    int
    readdir(const vector<string> &srcmounts,
            const string          dirname,
            vector<FileData>     &stats)
    {
      return _readdir(srcmounts,
                      dirname,
                      &stats,
                      stat_vector_filler);
    }

    int
    readdir(const char            *fusepath,
            void                  *buf,
            fuse_fill_dir_t        filler,
            off_t                  offset,
            struct fuse_file_info *fi)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);

      return _readdir(config.srcmounts,
                      fusepath,
                      buf,
                      filler);
    }
  }
}

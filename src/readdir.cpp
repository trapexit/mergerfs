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

#define _BSD_SOURCE

#include <fuse.h>

#include <string>
#include <set>
#include <vector>

#include <errno.h>
#include <dirent.h>

#include "config.hpp"
#include "fs_path.hpp"
#include "readdir.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using std::set;
using std::pair;

#define NO_OFFSET 0

static
int
_readdir(const vector<string>  &srcmounts,
         const string          &dirname,
         void                  *buf,
         const fuse_fill_dir_t  filler)
{
  set<string> found;
  struct stat st = {0};

  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      DIR *dh;
      string basepath;

      fs::path::make(srcmounts[i],dirname,basepath);
      dh = ::opendir(basepath.c_str());
      if(!dh)
        continue;

      for(struct dirent *de = ::readdir(dh); de != NULL; de = ::readdir(dh))
        {
          if(found.insert(de->d_name).second == false)
            continue;

          st.st_mode = DTTOIF(de->d_type);
          if(filler(buf,de->d_name,&st,NO_OFFSET) != 0)
            return -ENOMEM;
        }

      ::closedir(dh);
    }

  if(found.empty())
    return -ENOENT;

  return 0;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    readdir(const char      *fusepath,
            void            *buf,
            fuse_fill_dir_t  filler,
            off_t            offset,
            fuse_file_info  *fi)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _readdir(config.srcmounts,
                      fusepath,
                      buf,
                      filler);
    }
  }
}

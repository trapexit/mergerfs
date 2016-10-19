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

#define _BSD_SOURCE

#include <fuse.h>

#include <string>
#include <set>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_closedir.hpp"
#include "fs_base_opendir.hpp"
#include "fs_base_readdir.hpp"
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
         const char            *dirname,
         void                  *buf,
         const fuse_fill_dir_t  filler)
{
  set<string> found;
  struct stat st = {0};

  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      DIR *dh;
      string basepath;

      fs::path::make(&srcmounts[i],dirname,basepath);

      dh = fs::opendir(basepath);
      if(!dh)
        continue;

      for(struct dirent *de = fs::readdir(dh); de != NULL; de = fs::readdir(dh))
        {
          if(found.insert(de->d_name).second == false)
            continue;

          st.st_mode = DTTOIF(de->d_type);
          if(filler(buf,de->d_name,&st,NO_OFFSET) != 0)
            return -ENOMEM;
        }

      fs::closedir(dh);
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

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

#ifndef __MERGERFS_READDIR_HPP__
#define __MERGERFS_READDIR_HPP__

#include <fuse.h>

#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mergerfs
{
  namespace readdir
  {
    struct FileData
    {
      FileData(const std::string &filename_,
               const struct stat &stats_)
        : filename(filename_),
          stats(stats_)
      {}

      std::string filename;
      struct stat stats;
    };

    int
    readdir(const char            *fusepath,
            void                  *buf,
            fuse_fill_dir_t        filler,
            off_t                  offset,
            struct fuse_file_info *fi);

    int
    readdir(const std::vector<std::string> &srcmounts,
            const std::string              &dirname,
            std::vector<FileData>          &stats);
  }
}

#endif

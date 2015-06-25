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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>

#include <string>
#include <vector>

#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;

namespace mergerfs
{
  int
  Policy::Func::epmfs(const Category::Enum::Type  type,
                      const vector<string>       &basepaths,
                      const string               &fusepath,
                      const size_t                minfreespace,
                      Paths                      &paths)
  {
    fsblkcnt_t existingmfs;
    fsblkcnt_t generalmfs;
    string     fullpath;
    string     generalmfspath;
    string     existingmfspath;
    vector<string>::const_iterator iter  = basepaths.begin();
    vector<string>::const_iterator eiter = basepaths.end();

    if(iter == eiter)
      return (errno = ENOENT,-1);

    existingmfs = 0;
    generalmfs  = 0;
    do
      {
        int rv;
        struct statvfs  fsstats;
        const string   &mountpoint = *iter;

        rv = ::statvfs(mountpoint.c_str(),&fsstats);
        if(rv == 0)
          {
            fsblkcnt_t  spaceavail;
            struct stat st;

            spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
            if(spaceavail > generalmfs)
              {
                generalmfs     = spaceavail;
                generalmfspath = mountpoint;
              }

            fullpath = fs::make_path(mountpoint,fusepath);
            rv = ::lstat(fullpath.c_str(),&st);
            if(rv == 0)
              {
                if(spaceavail > existingmfs)
                  {
                    existingmfs     = spaceavail;
                    existingmfspath = mountpoint;
                  }
              }
          }

        ++iter;
      }
    while(iter != eiter);

    if(existingmfspath.empty())
      existingmfspath = generalmfspath;

    paths.push_back(Path(existingmfspath,
                         fullpath));

    return 0;
  }
}

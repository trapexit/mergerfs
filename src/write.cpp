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

#include <errno.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <sstream>

#include "config.hpp"
#include "policy.hpp"
#include "category.hpp"
#include "ugid.hpp"
#include "fileinfo.hpp"

using mergerfs::config::Config;
using mergerfs::Policy;
using mergerfs::Category;
using std::string;
using std::vector;
using std::stringstream;

static
int
_process_kv(Config       &config,
            const string  key,
            const string  value)
{
  const Category *cat;
  const Policy   *policy;

  cat = Category::find(key);
  if(cat == Category::invalid)
    return -EINVAL;

  policy = Policy::find(value);
  if(policy == Policy::invalid)
    return -EINVAL;

  config.policies[*cat] = policy;

  return 0;
}

static
int
_write_controlfile(Config       &config,
                   string       &existing,
                   const string  buf,
                   const off_t   _offset)
{
  size_t bufsize      = buf.size();
  size_t existingsize = existing.size();

  if((existingsize + buf.size()) > 1024)
    return (existing.clear(),-EINVAL);

  existing += buf;

  size_t nlpos = existing.find_first_of('\n',existingsize);
  if(nlpos == string::npos)
    return 0;

  string line = existing.substr(0,nlpos);
  existing = existing.substr(nlpos+1);

  size_t equalsoffset = line.find_first_of('=');
  if(equalsoffset == string::npos)
    return -EINVAL;

  int rv;
  string key   = line.substr(0,equalsoffset);
  string value = line.substr(equalsoffset+1);

  rv = _process_kv(config,key,value);

  return ((rv < 0) ? rv : bufsize);
}

static
int
_write(const int     fd,
       const void   *buf,
       const size_t  count,
       const off_t   offset)
{
  int rv;

  rv = ::pwrite(fd,buf,count,offset);

  return ((rv == -1) ? -errno : rv);
}

namespace mergerfs
{
  namespace write
  {
    int
    write(const char            *fusepath,
          const char            *buf,
          size_t                 count,
          off_t                  offset,
          struct fuse_file_info *fi)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _write_controlfile(config::get_writable(),
                                  *(string*)fi->fh,
                                  string(buf,count),
                                  offset);

      return _write(((FileInfo*)fi->fh)->fd,
                    buf,
                    count,
                    offset);
    }
  }
}

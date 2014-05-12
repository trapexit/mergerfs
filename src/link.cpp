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

#include <string>
#include <vector>

#include <errno.h>
#include <unistd.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "assert.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_link(const Policy::Action::Func  searchFunc,
      const vector<string>       &srcmounts,
      const string                from,
      const string                to)
{
  int rv;
  int error;
  vector<fs::Path> paths;

  searchFunc(srcmounts,from,paths);
  if(paths.empty())
    return -ENOENT;

  rv    = -1;
  error =  0;
  for(vector<fs::Path>::const_iterator
        i = paths.begin(), ei = paths.end(); i != ei; ++i)
    {
      const string pathfrom = fs::make_path(i->base,from);
      const string pathto   = fs::make_path(i->base,to);

      rv &= ::link(pathfrom.c_str(),pathto.c_str());
      if(rv == -1)
        error = errno;
    }

  return ((rv == -1) ? -error : 0);
}

namespace mergerfs
{
  namespace link
  {
    int
    link(const char *from,
         const char *to)
    {
      const ugid::SetResetGuard  ugid;
      const config::Config      &config = config::get();

      if(from == config.controlfile)
        return -EPERM;

      return _link(config.policy.action,
                   config.srcmounts,
                   from,
                   to);
    }
  }
}

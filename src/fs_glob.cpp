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

#include <stdint.h>

#include <glob.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace fs
{
  void
  glob(const vector<string> &patterns_,
       vector<string>       &strs_)
  {
    int flags;
    size_t veclen;
    glob_t gbuf = {0};

    veclen = patterns_.size();
    if(veclen == 0)
      return;

    flags = GLOB_NOCHECK;
    ::glob(patterns_[0].c_str(),flags,NULL,&gbuf);

    flags = GLOB_APPEND|GLOB_NOCHECK;
    for(size_t i = 1; i < veclen; i++)
      ::glob(patterns_[i].c_str(),flags,NULL,&gbuf);

    for(size_t i = 0; i < gbuf.gl_pathc; ++i)
      strs_.push_back(gbuf.gl_pathv[i]);

    ::globfree(&gbuf);
  }
}

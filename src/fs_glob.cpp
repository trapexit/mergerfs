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

#include <glob.h>

#include <cstdint>
#include <string>
#include <vector>

using std::string;
using std::vector;


namespace fs
{
  void
  glob(const string   &pattern_,
       vector<string> *strs_)
  {
    int flags;
    glob_t gbuf = {0};

    flags = GLOB_NOCHECK;
    ::glob(pattern_.c_str(),flags,NULL,&gbuf);

    for(size_t i = 0; i < gbuf.gl_pathc; i++)
      strs_->push_back(gbuf.gl_pathv[i]);

    ::globfree(&gbuf);
  }
}

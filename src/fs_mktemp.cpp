/*
  ISC License

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

#include "errno.hpp"
#include "fs_open.hpp"

#include <cstdlib>
#include <string>

#include <limits.h>

using std::string;

#define PADLEN 6
#define MAX_ATTEMPTS 10


static
string
generate_tmp_path(const string &base_)
{
  string tmp;

  tmp = base_;
  if((tmp.size() + PADLEN + 1) > PATH_MAX)
    tmp.resize(tmp.size() - PADLEN - 1);
  tmp += '.';
  for(int i = 0; i < PADLEN; i++)
    tmp += ('A' + (std::rand() % 26));

  return tmp;
}

namespace fs
{
  int
  mktemp(string    *base_,
         const int  flags_)
  {
    int fd;
    int count;
    int flags;
    string tmppath;

    fd    = -1;
    count = MAX_ATTEMPTS;
    flags = (flags_ | O_EXCL | O_CREAT);
    while(count-- > 0)
      {
        tmppath = generate_tmp_path(*base_);

        fd = fs::open(tmppath,flags,S_IWUSR);
        if((fd == -1) && (errno == EEXIST))
          continue;
        else if(fd != -1)
          *base_ = tmppath;

        return fd;
      }

    return (errno=EEXIST,-1);
  }
}

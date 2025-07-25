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

#include "fs_cow.hpp"

#include "errno.hpp"
#include "fs_lstat.hpp"
#include "fs_copyfile.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


bool
fs::cow::is_eligible(const int flags_)
{
  int accmode;

  accmode = (flags_ & O_ACCMODE);

  return ((accmode == O_RDWR) ||
          (accmode == O_WRONLY));
}

bool
fs::cow::is_eligible(const struct stat &st_)
{
  return ((S_ISREG(st_.st_mode)) && (st_.st_nlink > 1));
}

bool
fs::cow::is_eligible(const int          flags_,
                     const struct stat &st_)
{
  return (is_eligible(flags_) && is_eligible(st_));
}

bool
fs::cow::is_eligible(const char *fullpath_,
                     const int   flags_)
{
  int rv;
  struct stat st;

  if(!fs::cow::is_eligible(flags_))
    return false;

  rv = fs::lstat(fullpath_,&st);
  if(rv < 0)
    return false;

  return fs::cow::is_eligible(st);
}

int
fs::cow::break_link(const char *src_filepath_)
{
  return fs::copyfile(src_filepath_,
                      src_filepath_,
                      { .cleanup_failure = true });
}

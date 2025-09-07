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

#include "fs_mktemp.hpp"

#include "errno.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "rnd.hpp"

#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <tuple>

#define PAD_LEN             6ULL
#define MAX_ATTEMPTS        3

static char const CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static size_t const CHARS_SIZE = (sizeof(CHARS) - 1);

static
fs::path
_generate_tmp_path(const fs::path    &dirpath_,
                   const std::string &src_filename_)
{
  long name_max;
  size_t substr_len;
  std::string tmp_filename;

  name_max = ::pathconf(dirpath_.c_str(),_PC_NAME_MAX);
  if(name_max == -1)
    name_max = NAME_MAX;

  substr_len = std::min(src_filename_.size(),
                        (size_t)(name_max - PAD_LEN - 2ULL));

  tmp_filename = '.';
  tmp_filename += src_filename_.substr(0,substr_len);
  tmp_filename += '_';
  for(size_t i = 0; i < PAD_LEN; i++)
    tmp_filename += CHARS[RND::rand64(CHARS_SIZE)];

  return dirpath_ / tmp_filename;
}

std::tuple<int,fs::path>
fs::mktemp_in_dir(const fs::path &dirpath_,
                  const fs::path &filename_,
                  const int       flags_)
{
  int fd;
  int count;
  int flags;
  fs::path tmp_filepath;

  count = MAX_ATTEMPTS;
  flags = (flags_ | O_EXCL | O_CREAT);
  while(count-- > 0)
    {
      tmp_filepath = ::_generate_tmp_path(dirpath_,filename_);

      fd = fs::open(tmp_filepath,flags,S_IRUSR|S_IWUSR);
      if(fd == -EEXIST)
        continue;
      if(fd < 0)
        return std::make_tuple(fd,fs::path());

      return std::make_tuple(fd,tmp_filepath);
    }

  return std::make_tuple(-EEXIST,fs::path());
}

std::tuple<int,fs::path>
fs::mktemp(const fs::path &filepath_,
           const int       flags_)
{
  return fs::mktemp_in_dir(filepath_.parent_path(),
                           filepath_.filename(),
                           flags_);
}

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
#include "fs_path.hpp"
#include "rnd.hpp"

#include <limits.h>

#include <cstdlib>
#include <string>
#include <tuple>

#define PAD_LEN             16
#define MAX_ATTEMPTS        3

static char const   CHARS[]    = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static size_t const CHARS_SIZE = (sizeof(CHARS) - 1);


namespace l
{
  static
  std::string
  generate_tmp_path(std::string const base_)
  {
    fs::Path path;
    std::string filename;

    filename  = '.';
    for(int i = 0; i < PAD_LEN; i++)
      filename += CHARS[RND::rand64(CHARS_SIZE)];

    path = base_;
    path /= filename;

    return path.string();
  }
}

namespace fs
{
  std::tuple<int,std::string>
  mktemp_in_dir(std::string const dirpath_,
                int const         flags_)
  {
    int fd;
    int count;
    int flags;
    std::string tmp_filepath;

    fd    = -1;
    count = MAX_ATTEMPTS;
    flags = (flags_ | O_EXCL | O_CREAT | O_TRUNC);
    while(count-- > 0)
      {
        tmp_filepath = l::generate_tmp_path(dirpath_);

        fd = fs::open(tmp_filepath,flags,S_IWUSR);
        if((fd == -1) && (errno == EEXIST))
          continue;
        if(fd == -1)
          return std::make_tuple(-errno,std::string());

        return std::make_tuple(fd,tmp_filepath);
      }

    return std::make_tuple(-EEXIST,std::string());
  }

  std::tuple<int,std::string>
  mktemp(std::string const filepath_,
         int const         flags_)
  {
    ghc::filesystem::path filepath{filepath_};

    return fs::mktemp_in_dir(filepath.parent_path(),flags_);
  }
}

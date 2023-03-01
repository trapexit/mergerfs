/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_readahead.hpp"

#include "fmt/core.h"
#include "fs_lstat.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#include <sys/sysmacros.h>

namespace l
{
  static
  std::string
  generate_readahead_sys_path(const std::uint64_t major_,
                              const std::uint64_t minor_)
  {
    return fmt::format("/sys/class/bdi/{}:{}/read_ahead_kb",major_,minor_);
  }
}

int
fs::readahead(const std::uint64_t major_dev_,
              const std::uint64_t minor_dev_,
              const std::uint64_t size_in_kb_)
{
  std::string syspath;
  std::ofstream ofs;

  syspath = l::generate_readahead_sys_path(major_dev_,minor_dev_);

  ofs.open(syspath);
  if(ofs)
    {
      ofs << fmt::format("{}\n",size_in_kb_);
      ofs.close();
    }

  return 0;
}

int
fs::readahead(const std::uint64_t dev_,
              const std::uint64_t size_in_kb_)
{
  std::uint32_t major_dev;
  std::uint32_t minor_dev;

  major_dev = major(dev_);
  minor_dev = minor(dev_);

  return fs::readahead(major_dev,minor_dev,size_in_kb_);
}

int
fs::readahead(const std::string   path_,
              const std::uint64_t size_in_kb_)
{
  int rv;
  struct stat st;

  rv = fs::lstat(path_,&st);
  if(rv == -1)
    return -errno;

  return fs::readahead(st.st_dev,size_in_kb_);
}

/*
  ISC License

  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include "fuse_kernel.h"

#include <cstdint>
#include <string>

#include <sys/stat.h>


namespace fs
{
  namespace inode
  {
    static const uint64_t MAGIC = 0x7472617065786974;

    int set_algo(const std::string &s);
    std::string get_algo(void);

    uint64_t calc(const char     *fusepath,
                  const uint64_t  fusepath_len,
                  const mode_t    mode,
                  const dev_t     dev,
                  const ino_t     ino);
    uint64_t calc(std::string const &fusepath,
                  mode_t const       mode,
                  dev_t const        dev,
                  ino_t              ino);

    void calc(const char     *fusepath,
              const uint64_t  fusepath_len,
              struct stat    *st);
    void calc(const char        *fusepath,
              const uint64_t     fusepath_len,
              struct fuse_statx *st);

    void calc(const char  *fusepath,
              struct stat *st);
    void calc(const char        *fusepath,
              struct fuse_statx *st);

    void calc(const std::string &fusepath,
              struct stat       *st);
  }
}

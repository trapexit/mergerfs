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

#include "fs_info_t.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "fs_statvfs.hpp"
#include "fs_statvfs_cache.hpp"
#include "statvfs_util.hpp"

#include <cstdint>
#include <string>

using std::string;


namespace fs
{
  int
  info(const string &path_,
       fs::info_t   *info_)
  {
    int rv;
    struct statvfs st;

    rv = fs::statvfs_cache(path_.c_str(),&st);
    if(rv == 0)
      {
        info_->readonly   = StatVFS::readonly(st);
        info_->spaceavail = StatVFS::spaceavail(st);
        info_->spaceused  = StatVFS::spaceused(st);
      }

    return rv;
  }
}

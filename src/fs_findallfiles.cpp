/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_findallfiles.hpp"

#include "fs_exists.hpp"
#include "fs_path.hpp"

#include <string>
#include <vector>


void
fs::findallfiles(const std::vector<std::string> &basepaths_,
                 const char                     *fusepath_,
                 std::vector<std::string>       *paths_)
{
  std::string fullpath;

  for(const auto &basepath : basepaths_)
    {
      fullpath = fs::path::make(basepath,fusepath_);

      if(!fs::exists(fullpath))
        continue;

      paths_->push_back(fullpath);
    }
}

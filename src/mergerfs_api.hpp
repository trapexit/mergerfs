/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_path.hpp"

#include <map>
#include <string>
#include <vector>


namespace mergerfs
{
  namespace api
  {
    bool
    is_mergerfs(const fs::path &path);

    int
    get_kvs(const fs::path                    &mountpoint,
            std::map<std::string,std::string> *kvs);

    int
    basepath(const std::string &path,
             std::string       &basepath);
    int
    relpath(const std::string &path,
            std::string       &relpath);
    int
    fullpath(const std::string &path,
             std::string       &fullpath);
    int
    allpaths(const std::string        &path,
             std::vector<std::string> &paths);
  }
}

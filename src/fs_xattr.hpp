/*
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

#include "ghc/filesystem.hpp"

#include <string>
#include <vector>
#include <map>


namespace fs
{
  namespace xattr
  {
    int list(const ghc::filesystem::path &path,
             std::vector<char>           *attrs);
    int list(const ghc::filesystem::path &path,
             std::string                 *attrs);
    int list(const ghc::filesystem::path &path,
             std::vector<std::string>    *attrs);

    int get(const ghc::filesystem::path &path,
            const std::string           &attr,
            std::vector<char>           *value);
    int get(const ghc::filesystem::path &path,
            const std::string           &attr,
            std::string                 *value);

    int get(const ghc::filesystem::path       &path,
            std::map<std::string,std::string> *attrs);

    int set(const ghc::filesystem::path &path,
            const std::string           &key,
            const std::string           &value,
            const int                    flags);
    int set(const int          fd,
            const std::string &key,
            const std::string &value,
            const int          flags);

    int set(const ghc::filesystem::path             &path,
            const std::map<std::string,std::string> &attrs);

    int copy(const int fdin,
             const int fdout);
    int copy(const ghc::filesystem::path &from,
             const ghc::filesystem::path &to);
  }
}

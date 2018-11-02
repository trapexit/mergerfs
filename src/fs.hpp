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

#include <string>
#include <vector>

#include <fcntl.h>
#include <stdint.h>

namespace fs
{
  using std::string;
  using std::vector;

  int readonly(const string *path_,
               bool         *readonly_);

  int spaceavail(const string *path_,
                 uint64_t     *spaceavail_);

  int spaceused(const string *path_,
                uint64_t     *spaceavail_);

  void findallfiles(const vector<string> &basepaths,
                    const char           *fusepath,
                    vector<string>       &paths);

  int findonfs(const vector<string> &basepaths,
               const string         &fusepath,
               const int             fd,
               string               &basepath);

  void realpathize(vector<string> &strs);

  int getfl(const int fd);
  int setfl(const int    fd,
            const mode_t mode);

  int mfs(const vector<string> &srcs,
          const uint64_t        minfreespace,
          string               &path);
};

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

  void findallfiles(const vector<string> &basepaths_,
                    const char           *fusepath_,
                    vector<string>       &paths_);

  int findonfs(const vector<string> &basepaths_,
               const string         &fusepath_,
               const int             fd_,
               string               &basepath_);

  void realpathize(vector<string> &strs_);

  int getfl(const int fd_);
  int setfl(const int    fd_,
            const mode_t mode_);

  int mfs(const vector<string> &srcs_,
          const uint64_t        minfreespace_,
          string               &path_);
}

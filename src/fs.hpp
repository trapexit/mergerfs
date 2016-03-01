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

#ifndef __FS_HPP__
#define __FS_HPP__

#include <string>
#include <vector>

namespace fs
{
  using std::size_t;
  using std::string;
  using std::vector;

  bool exists(const string &path,
              struct stat  &st);
  bool exists(const string &path);

  bool statvfs(const string   &path,
               struct statvfs &st);

  bool info(const string &path,
            bool         &readonly,
            size_t       &spaceavail);

  bool readonly(const string &path);

  bool spaceavail(const string &path,
                  size_t       &spaceavail);

  void findallfiles(const vector<string> &srcmounts,
                    const char           *fusepath,
                    vector<string>       &paths);

  int findonfs(const vector<string> &srcmounts,
               const char           *fusepath,
               const int             fd,
               string               &basepath);

  void glob(const vector<string> &patterns,
            vector<string>       &strs);

  void realpathize(vector<string> &strs);

  int getfl(const int fd);

  int mfs(const vector<string> &srcs,
          const size_t          minfreespace,
          string               &path);
};

#endif // __FS_HPP__

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

#include <set>
#include <string>
#include <vector>

namespace str
{
  void
  split(const char               *str,
        const char                delimiter,
        std::vector<std::string> *result);
  void
  split(const std::string        &str,
        const char                delimiter,
        std::vector<std::string> *result);

  void
  rsplit1(const std::string        &str,
          const char                delimiter,
          std::vector<std::string> *result);

  void
  splitkv(const std::string &str,
          const char         delimiter,
          std::string       *key,
          std::string       *value);

  std::string
  join(const std::vector<std::string> &vec,
       const size_t                    substridx,
       const char                      sep);

  std::string
  join(const std::vector<std::string> &vec,
       const char                      sep);

  std::string
  join(const std::set<std::string> &s,
       const char                   sep);

  size_t
  longest_common_prefix_index(const std::vector<std::string> &vec);

  std::string
  longest_common_prefix(const std::vector<std::string> &vec);

  std::string
  remove_common_prefix_and_join(const std::vector<std::string> &vec,
                                const char                      sep);

  void
  erase_fnmatches(const std::vector<std::string> &pattern,
                  std::vector<std::string>       &strs);

  bool
  isprefix(const std::string &s0,
           const std::string &s1);

  bool
  startswith(const std::string &str_,
             const std::string &prefix_);

  bool
  endswith(const std::string &str_,
           const std::string &suffix_);

  std::string
  trim(const std::string &str);
}

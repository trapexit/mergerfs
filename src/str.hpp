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

#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace str
{
  [[nodiscard]]
  std::vector<std::string>
  split(const std::string_view str,
        const char delimiter);

  [[nodiscard]]
  std::set<std::string>
  split_to_set(const std::string_view str,
               const char delimiter);

  [[nodiscard]]
  std::vector<std::string>
  split_on_null(const std::string_view str);

  [[nodiscard]]
  std::vector<std::string>
  lsplit1(const std::string_view str,
          const char delimiter);

  [[nodiscard]]
  std::vector<std::string>
  rsplit1(const std::string_view str,
          const char delimiter);

  [[nodiscard]]
  std::pair<std::string, std::string>
  splitkv(const std::string_view str,
          const char delimiter);

  [[nodiscard]]
  std::string
  join(const std::vector<std::string> &vec,
       const size_t substridx,
       const char sep);

  [[nodiscard]]
  std::string
  join(const std::vector<std::string> &vec,
       const char sep);

  [[nodiscard]]
  std::string
  join(const std::set<std::string> &s,
       const char sep);

  [[nodiscard]]
  size_t
  longest_common_prefix_index(const std::vector<std::string> &vec);

  [[nodiscard]]
  std::string
  longest_common_prefix(const std::vector<std::string> &vec);

  [[nodiscard]]
  std::string
  remove_common_prefix_and_join(const std::vector<std::string> &vec,
                                const char sep);

  void
  erase_fnmatches(const std::vector<std::string> &patterns,
                  std::vector<std::string> &strs);

  [[nodiscard]]
  bool
  startswith(const std::string_view str,
             const std::string_view prefix) noexcept;

  [[nodiscard]]
  bool
  startswith(const char *str,
             const char *prefix) noexcept;

  [[nodiscard]]
  bool
  endswith(const std::string_view str,
           const std::string_view suffix) noexcept;

  [[nodiscard]]
  std::string
  trim(const std::string_view str);

  [[nodiscard]]
  bool
  eq(const char *s0,
     const char *s1) noexcept;

  [[nodiscard]]
  std::string
  replace(const std::string_view str,
          const char src,
          const char dst);
}

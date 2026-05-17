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

#include "fs_cleanpath.hpp"

#include <string>


void
fs::cleanpath(std::string *path_)
{
  std::string &s = *path_;
  std::size_t w = 0;
  bool prev_slash = false;

  for(std::size_t r = 0; r < s.size(); r++)
    {
      const char c = s[r];
      if(c == '/')
        {
          if(prev_slash)
            continue;
          prev_slash = true;
        }
      else
        {
          prev_slash = false;
        }
      s[w++] = c;
    }

  // Strip trailing '/' but leave a bare "/" intact.
  if(w > 1 && s[w - 1] == '/')
    w--;

  s.resize(w);
}

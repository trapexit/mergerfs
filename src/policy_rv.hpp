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

#pragma once

#include <string>
#include <vector>

struct PolicyRV
{
  struct RV
  {
    RV(const int          rv_,
       const std::string &basepath_)
      : rv(rv_),
        basepath(basepath_)
    {
    }

    int         rv;
    std::string basepath;
  };

  std::vector<RV> successes;
  std::vector<RV> errors;

  void
  insert(const int          rv_,
         const std::string &basepath_)
  {
    if(rv_ >= 0)
      successes.push_back({rv_,basepath_});
    else
      errors.push_back({rv_,basepath_});
  }

  int
  get_error(const std::string &basepath_)
  {
    for(const auto &s : successes)
      {
        if(s.basepath == basepath_)
          return s.rv;
      }

    for(const auto &e : errors)
      {
        if(e.basepath == basepath_)
          return e.rv;
      }

    return -ENOENT;
  }
};

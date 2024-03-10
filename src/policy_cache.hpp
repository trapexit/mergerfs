/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "nonstd/optional.hpp"
#include "boost/unordered/concurrent_flat_map.hpp"

#include "fmt/core.h"

#include <cstdint>
#include <string>


class PolicyCache
{
public:
  typedef boost::concurrent_flat_map<std::string,std::string> Map;

public:
  PolicyCache()
    : _max_size(1)
  {
  }

public:
  void
  insert(std::string const &key_,
         std::string const &val_)
  {
    auto size = _cache.size();
    
    _cache.visit_while([&](Map::value_type const &v_)
    {
      //      _cache.erase(v_.first);
      --size;
      fmt::print("{} > {}\n",size,_max_size);
      return (size > _max_size);
    });
    
    _cache.insert_or_assign(key_,val_);
  }

  const
  nonstd::optional<std::string>
  find(std::string const key_)
  {
    nonstd::optional<std::string> rv;

    _cache.visit(key_,
                 [&](Map::value_type &v_)
                 {
                   rv = v_.second;
                 });

    return rv;
  }

private:
  unsigned _max_size;
  Map      _cache;
};

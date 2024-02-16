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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <boost/unordered/unordered_flat_map.hpp>


class PolicyCache
{
public:
  typedef boost::unordered_flat_map<std::string,std::string> Map;

public:
  PolicyCache()
    : _max_size(256)
  {
  }

public:
  void
  insert(std::string const &key_,
         std::string       &val_)
  {
    if(_cache.size() > _max_size)
      _cache.erase(_cache.begin());
    _cache.insert({key_,val_});
  }

  const
  std::string*
  find(char const *key_)
  {
    Map::const_iterator i;

    i = _cache.find(key_);
    if(i == _cache.end())
      return NULL;

    return &i->second;
  }

  void
  erase(const char *key_)
  {
    _cache.erase(key_);
  }

  void
  cleanup()
  {
    while(_cache.size() > _max_size)
      _cache.erase(_cache.begin());
  }

  void
  clear()
  {

  }

private:
  unsigned _max_size;
  Map      _cache;
};

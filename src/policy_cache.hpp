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

// This uses a hashed key to limit the size of the data
// structure. This could lead to choosing the wrong value but given
// its usecase that really isn't a big deal.

#include "nonstd/optional.hpp"
#include "boost/unordered/concurrent_flat_map.hpp"
#include "boost/flyweight.hpp"
#include "boost/flyweight/no_tracking.hpp"
#include "wyhash.h"
#include "strpool.h"

#include "fmt/core.h"

#include <cstdint>
#include <string>


class PolicyCache
{
public:
  typedef uint64_t Key;
  typedef boost::flyweight<std::string,boost::flyweights::no_tracking> Val;
  typedef boost::concurrent_flat_map<Key,Val> Map;

public:
  PolicyCache(unsigned const max_size_ = 256)
    : _max_size(max_size_)
  {

  }

  ~PolicyCache()
  {

  }

public:
  Val
  insert(char const        *key_,
         size_t const       keylen_,
         std::string const &val_)
  {
    Val val;
    uint64_t hash;

    val = val_;
    hash = wyhash(key_,keylen_,0xdeadbeef,_wyp);
    _cache.insert_or_assign(hash,val_);

    fmt::print("insert {}={}\n",
               key_,
               val.get());

    return val;
  }

  Val
  insert(char const        *key_,
         std::string const &val_)
  {
    return insert(key_,
                  strlen(key_),
                  val_);
  }

  Val
  insert(std::string const &key_,
         std::string const &val_)
  {
    return insert(key_.c_str(),
                  key_.size(),
                  val_);
  }

  Val
  find(std::string const &key_) const
  {
    Val rv;    
    uint64_t hash;

    hash = wyhash(key_.c_str(),key_.size(),0xdeadbeef,_wyp);
    _cache.cvisit(hash,
                  [&](Map::value_type const &v_)
                  {
                    fmt::print("get {}={}\n",
                               key_,
                               v_.second.get());
                    rv = v_.second;
                  });

    return rv;
  }

  void
  erase(std::string const &key_)
  {
    uint64_t hash;

    hash = wyhash(key_.c_str(),key_.size(),0xdeadbeef,_wyp);

    _cache.erase(hash);
  }

private:
  unsigned  _max_size;
  Map       _cache;
};

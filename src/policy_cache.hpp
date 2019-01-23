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

#include "policy.hpp"

#include <string>
#include <map>

#include <pthread.h>
#include <stdint.h>

class PolicyCache
{
public:
  struct Value
  {
    Value();

    uint64_t    time;
    std::string path;
  };

public:
  PolicyCache(void);

public:
  void erase(const char *fusepath_);
  void cleanup(const int prob_ = 1);
  void clear(void);

public:
  int operator()(Policy::Func::Search &func_,
                 const Branches       &branches_,
                 const char           *fusepath_,
                 const uint64_t        minfreespace_,
                 std::string          *branch_);

public:
  uint64_t timeout;

private:
  pthread_mutex_t             _lock;
  std::map<std::string,Value> _cache;
};

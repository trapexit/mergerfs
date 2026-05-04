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

#include "to_string.hpp"
#include "from_string.hpp"
#include "tofrom_string.hpp"

#include <atomic>


template<typename T>
class TFSRef : public ToFromString
{
public:
  int
  from_string(const std::string_view s_) final
  {
    return str::from(s_,&_data);
  }

  std::string
  to_string(void) const final
  {
    return str::to(_data);
  }

public:
  TFSRef(T &data_)
    : _data(data_)
  {
  }

private:
  T &_data;
};

template<typename T>
class TFSRef<std::atomic<T>> : public ToFromString
{
public:
  int
  from_string(const std::string_view s_) final
  {
    T value;
    int rv;

    rv = str::from(s_,&value);
    if(rv < 0)
      return rv;

    _data.store(value,std::memory_order_relaxed);

    return 0;
  }

  std::string
  to_string(void) const final
  {
    return str::to(_data.load(std::memory_order_relaxed));
  }

public:
  TFSRef(std::atomic<T> &data_)
    : _data(data_)
  {
  }

private:
  std::atomic<T> &_data;
};

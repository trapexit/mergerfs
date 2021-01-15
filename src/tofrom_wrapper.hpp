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

#include "from_string.hpp"
#include "to_string.hpp"
#include "tofrom_string.hpp"

#include <errno.h>

template<typename T>
class ToFromWrapper : public ToFromString
{
public:
  int
  from_string(const std::string &s_) final
  {
    return str::from(s_,&_data);
  }

  std::string
  to_string(void) const final
  {
    return str::to(_data);
  }

public:
  ToFromWrapper<T>()
  {
  }

  ToFromWrapper<T>(const T data_)
    : _data(data_)
  {
  }

public:
  ToFromWrapper<T>&
  operator=(const T &data_)
  {
    _data = data_;
    return *this;
  }

public:
  operator const T&() const
  {
    return _data;
  }

  T*
  operator->()
  {
    return &_data;
  }

  const
  T*
  operator->() const
  {
    return &_data;
  }

public:
  bool
  operator==(const T &data_) const
  {
    return (_data == data_);
  }

private:
  T _data;
};

template<typename T>
class ROToFromWrapper : public ToFromString
{
public:
  int
  from_string(const std::string &s_) final
  {
    return -EINVAL;
  }

  std::string
  to_string(void) const final
  {
    return str::to(_data);
  }

public:
  ROToFromWrapper<T>()
  {
  }

  ROToFromWrapper<T>(const T data_)
    : _data(data_)
  {
  }

public:
  operator T() const
  {
    return _data;
  }

  T*
  operator->()
  {
    return &_data;
  }

public:
  bool
  operator==(const T &data_) const
  {
    return (_data == data_);
  }

private:
  T _data;
};

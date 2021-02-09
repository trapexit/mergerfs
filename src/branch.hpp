/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "strvec.hpp"
#include "tofrom_string.hpp"

#include <cstdint>
#include <string>
#include <vector>


class Branch final : public ToFromString
{
public:
  typedef std::vector<Branch> Vector;

public:
  Branch(const uint64_t &default_minfreespace_);

public:
  enum class Mode
    {
     INVALID,
     RO,
     RW,
     NC
    };

public:
  bool ro(void) const;
  bool nc(void) const;
  bool ro_or_nc(void) const;

public:
  int from_string(const std::string &str) final;
  std::string to_string(void) const final;

public:
  uint64_t minfreespace() const;
  void set_minfreespace(const uint64_t);

public:
  Mode mode;
  std::string path;

private:
  nonstd::optional<uint64_t>  _minfreespace;
  const uint64_t             *_default_minfreespace;
};

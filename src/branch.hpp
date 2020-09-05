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

#include "rwlock.hpp"
#include "tofrom_string.hpp"
#include "nonstd/optional.hpp"

#include <string>
#include <vector>

#include <stdint.h>
#include <pthread.h>

class Branch : public ToFromString
{
public:
  Branch(const uint64_t &default_minfreespace);

public:
  int from_string(const std::string &str);
  std::string to_string(void) const;

public:
  enum class Mode
    {
      INVALID,
      RO,
      RW,
      NC
    };

public:
  Mode        mode;
  std::string path;
  uint64_t    minfreespace() const;

public:
  void set_minfreespace(const uint64_t minfreespace);

public:
  bool ro(void) const;
  bool nc(void) const;
  bool ro_or_nc(void) const;

private:
  nonstd::optional<uint64_t>  _minfreespace;
  const uint64_t             *_default_minfreespace;
};

typedef std::vector<Branch> BranchVec;

class Branches : public ToFromString
{
public:
  Branches(const uint64_t &default_minfreespace_);

public:
  int from_string(const std::string &str);
  std::string to_string(void) const;

public:
  void to_paths(std::vector<std::string> &vec) const;

public:
  mutable pthread_rwlock_t lock;
  BranchVec vec;
  const uint64_t &default_minfreespace;
};

class SrcMounts : public ToFromString
{
public:
  SrcMounts(Branches &b_);

public:
  int from_string(const std::string &str);
  std::string to_string(void) const;

private:
  Branches &_branches;
};

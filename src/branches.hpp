/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "branch.hpp"
#include "nonstd/optional.hpp"
#include "strvec.hpp"
#include "tofrom_string.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>


class Branches final : public ToFromString
{
public:
  class Impl final : public ToFromString, public Branch::Vector
  {
  public:
    typedef std::shared_ptr<Impl> Ptr;
    typedef std::shared_ptr<const Impl> CPtr;

  public:
    Impl(const uint64_t &default_minfreespace_);

  public:
    int from_string(const std::string &str) final;
    std::string to_string(void) const final;

  public:
    const uint64_t& minfreespace(void) const;
    void to_paths(StrVec &strvec) const;

  public:
    Impl& operator=(Impl &impl_);
    Impl& operator=(Impl &&impl_);

  private:
    const uint64_t &_default_minfreespace;
  };

public:
  typedef Branches::Impl::Ptr  Ptr;
  typedef Branches::Impl::CPtr CPtr;

public:
  Branches(const uint64_t &default_minfreespace_)
    : _impl(std::make_shared<Impl>(default_minfreespace_))
  {}

public:
  int from_string(const std::string &str) final;
  std::string to_string(void) const final;

public:
  operator CPtr()   const { std::lock_guard<std::mutex> lg(_mutex); return _impl; }
  CPtr operator->() const { std::lock_guard<std::mutex> lg(_mutex); return _impl; }

private:
  mutable std::mutex _mutex;
  Ptr                _impl;
};

class SrcMounts : public ToFromString
{
public:
  SrcMounts(Branches &b_);

public:
  int from_string(const std::string &str) final;
  std::string to_string(void) const final;

private:
  Branches &_branches;
};

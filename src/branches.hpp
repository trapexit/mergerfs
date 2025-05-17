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
#include "fs_pathvector.hpp"
#include "strvec.hpp"
#include "tofrom_string.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

class Branches final : public ToFromString
{
public:
  class Impl final : public ToFromString,
                     public std::vector<Branch>
  {
  private:
    const u64 &_default_minfreespace;

  public:
    using Ptr  = std::shared_ptr<Impl>;

  public:
    Impl(const u64 &default_minfreespace);

  public:
    int from_string(const std::string &str) final;
    std::string to_string(void) const final;

  public:
    const u64 &minfreespace(void) const;
    void to_paths(StrVec &strvec) const;
    fs::PathVector to_paths() const;

  public:
    Impl& operator=(Impl &impl_);
    Impl& operator=(Impl &&impl_);
  };


public:
  using Ptr = Branches::Impl::Ptr;

private:
  mutable std::mutex _mutex;
  Ptr                _impl;

public:
  Branches(const u64 &default_minfreespace_)
    : _impl(std::make_shared<Impl>(default_minfreespace_))
  {}

public:
  int from_string(const std::string &str) final;
  std::string to_string(void) const final;

public:
  operator Ptr()   const { std::lock_guard<std::mutex> lg(_mutex); return _impl; }
  Ptr operator->() const { std::lock_guard<std::mutex> lg(_mutex); return _impl; }

public:
  void find_and_set_mode_ro();
};

class SrcMounts : public ToFromString
{
public:
  SrcMounts(Branches &b);

public:
  int from_string(const std::string &str) final;
  std::string to_string(void) const final;

private:
  Branches &_branches;
};

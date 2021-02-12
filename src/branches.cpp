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

#include "branches.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "from_string.hpp"
#include "fs_glob.hpp"
#include "fs_realpathize.hpp"
#include "nonstd/optional.hpp"
#include "num.hpp"
#include "str.hpp"

#include <string>

#include <fnmatch.h>

using std::string;
using std::vector;
using nonstd::optional;


Branches::Impl::Impl(const uint64_t &default_minfreespace_)
  : _default_minfreespace(default_minfreespace_)
{
}

Branches::Impl&
Branches::Impl::operator=(Branches::Impl &rval_)
{
  auto this_base = dynamic_cast<Branch::Vector*>(this);
  auto rval_base = dynamic_cast<Branch::Vector*>(&rval_);

  *this_base = *rval_base;

  return *this;
}

Branches::Impl&
Branches::Impl::operator=(Branches::Impl &&rval_)
{
  auto this_base = dynamic_cast<Branch::Vector*>(this);
  auto rval_base = dynamic_cast<Branch::Vector*>(&rval_);

  *this_base = std::move(*rval_base);

  return *this;
}

const
uint64_t&
Branches::Impl::minfreespace(void) const
{
  return _default_minfreespace;
}

namespace l
{
  static
  void
  split(const std::string &s_,
        std::string       *instr_,
        std::string       *values_)
  {
    uint64_t offset;

    offset = s_.find_first_not_of("+<>-=");
    if(offset > 1)
      offset = 2;
    *instr_ = s_.substr(0,offset);
    if(offset != std::string::npos)
      *values_ = s_.substr(offset);
  }

  static
  int
  parse_mode(const string &str_,
             Branch::Mode *mode_)
  {
    if(str_ == "RW")
      *mode_ = Branch::Mode::RW;
    ef(str_ == "RO")
      *mode_ = Branch::Mode::RO;
    ef(str_ == "NC")
      *mode_ = Branch::Mode::NC;
    else
      return -EINVAL;

    return 0;
  }

  static
  int
  parse_minfreespace(const string       &str_,
                     optional<uint64_t> *minfreespace_)
  {
    int rv;
    uint64_t uint64;

    rv = str::from(str_,&uint64);
    if(rv < 0)
      return rv;

    *minfreespace_ = uint64;

    return 0;
  }

  static
  int
  parse_branch(const string       &str_,
               string             *glob_,
               Branch::Mode       *mode_,
               optional<uint64_t> *minfreespace_)
  {
    int rv;
    string options;
    vector<string> v;

    str::rsplit1(str_,'=',&v);
    switch(v.size())
      {
      case 1:
        *glob_ = v[0];
        *mode_ = Branch::Mode::RW;
        break;
      case 2:
        *glob_  = v[0];
        options = v[1];
        v.clear();
        str::split(options,',',&v);
        switch(v.size())
          {
          case 2:
            rv = l::parse_minfreespace(v[1],minfreespace_);
            if(rv < 0)
              return rv;
          case 1:
            rv = l::parse_mode(v[0],mode_);
            if(rv < 0)
              return rv;
            break;
          case 0:
            return -EINVAL;
          }
        break;
      default:
        return -EINVAL;
      }

    return 0;
  }

  static
  int
  parse(const string   &str_,
        Branches::Impl *branches_)
  {
    int rv;
    string glob;
    StrVec paths;
    optional<uint64_t> minfreespace;
    Branch branch(branches_->minfreespace());

    rv = l::parse_branch(str_,&glob,&branch.mode,&minfreespace);
    if(rv < 0)
      return rv;

    if(minfreespace.has_value())
      branch.set_minfreespace(minfreespace.value());

    fs::glob(glob,&paths);
    fs::realpathize(&paths);
    for(auto &path : paths)
      {
        branch.path = path;
        branches_->push_back(branch);
      }

    return 0;
  }

  static
  int
  set(const std::string &str_,
      Branches::Impl    *branches_)
  {
    int rv;
    StrVec paths;
    Branches::Impl tmp_branches(branches_->minfreespace());

    str::split(str_,':',&paths);
    for(auto &path : paths)
      {
        rv = l::parse(path,&tmp_branches);
        if(rv < 0)
          return rv;
      }

    *branches_ = std::move(tmp_branches);

    return 0;
  }

  static
  int
  add_begin(const std::string &str_,
            Branches::Impl    *branches_)
  {
    int rv;
    vector<string> paths;
    Branches::Impl tmp_branches(branches_->minfreespace());

    str::split(str_,':',&paths);
    for(auto &path : paths)
      {
        rv = l::parse(path,&tmp_branches);
        if(rv < 0)
          return rv;
      }

    branches_->insert(branches_->begin(),
                      tmp_branches.begin(),
                      tmp_branches.end());

    return 0;
  }

  static
  int
  add_end(const std::string &str_,
          Branches::Impl    *branches_)
  {
    int rv;
    StrVec paths;
    Branches::Impl tmp_branches(branches_->minfreespace());

    str::split(str_,':',&paths);
    for(auto &path : paths)
      {
        rv = l::parse(path,&tmp_branches);
        if(rv < 0)
          return rv;
      }

    branches_->insert(branches_->end(),
                      tmp_branches.begin(),
                      tmp_branches.end());

    return 0;
  }

  static
  int
  erase_begin(Branches::Impl *branches_)
  {
    branches_->erase(branches_->begin());

    return 0;
  }

  static
  int
  erase_end(Branches::Impl *branches_)
  {
    branches_->pop_back();

    return 0;
  }

  static
  int
  erase_fnmatch(const std::string &str_,
                Branches::Impl    *branches_)
  {
    StrVec patterns;

    str::split(str_,':',&patterns);
    for(auto i = branches_->begin(); i != branches_->end();)
      {
        int match = FNM_NOMATCH;

        for(auto pi = patterns.cbegin(); pi != patterns.cend() && match != 0; ++pi)
          {
            match = ::fnmatch(pi->c_str(),i->path.c_str(),0);
          }

        i = ((match == 0) ? branches_->erase(i) : (i+1));
      }

    return 0;
  }
}

int
Branches::Impl::from_string(const std::string &s_)
{
  std::string instr;
  std::string values;

  l::split(s_,&instr,&values);

  if(instr == "+")
    return l::add_end(values,this);
  if(instr == "+<")
    return l::add_begin(values,this);
  if(instr == "+>")
    return l::add_end(values,this);
  if(instr == "-")
    return l::erase_fnmatch(values,this);
  if(instr == "-<")
    return l::erase_begin(this);
  if(instr == "->")
    return l::erase_end(this);
  if(instr == "=")
    return l::set(values,this);
  if(instr.empty())
    return l::set(values,this);

  return -EINVAL;
}

std::string
Branches::Impl::to_string(void) const
{
  string tmp;

  if(empty())
    return tmp;

  for(auto &branch : *this)
    {
      tmp += branch.to_string();
      tmp += ':';
    }

  tmp.pop_back();

  return tmp;
}

void
Branches::Impl::to_paths(StrVec &paths_) const
{
  for(auto &branch : *this)
    {
      paths_.push_back(branch.path);
    }
}

int
Branches::from_string(const std::string &str_)
{
  int rv;
  Branches::Ptr impl;
  Branches::Ptr new_impl;

  {
    std::lock_guard<std::mutex> lock_guard(_mutex);
    impl = _impl;
  }

  new_impl = std::make_shared<Branches::Impl>(impl->minfreespace());
  *new_impl = *impl;

  rv = new_impl->from_string(str_);
  if(rv < 0)
    return rv;

  {
    std::lock_guard<std::mutex> lock_guard(_mutex);
    _impl = new_impl;
  }

  return 0;
}

string
Branches::to_string(void) const
{
  std::lock_guard<std::mutex> lock_guard(_mutex);

  return _impl->to_string();
}

SrcMounts::SrcMounts(Branches &b_)
  : _branches(b_)
{

}

int
SrcMounts::from_string(const std::string &s_)
{
  return _branches.from_string(s_);
}

std::string
SrcMounts::to_string(void) const
{
  std::string rv;
  Branches::CPtr branches = _branches;

  if(branches->empty())
    return rv;

  for(const auto &branch : *branches)
    {
      rv += branch.path;
      rv += ':';
    }

  rv.pop_back();

  return rv;
}

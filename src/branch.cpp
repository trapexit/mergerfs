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

#include "branch.hpp"
#include "ef.hpp"
#include "from_string.hpp"
#include "fs_glob.hpp"
#include "fs_realpathize.hpp"
#include "nonstd/optional.hpp"
#include "num.hpp"
#include "str.hpp"

#include <string>

#include <errno.h>
#include <fnmatch.h>

using std::string;
using std::vector;
using nonstd::optional;


Branch::Branch(const uint64_t &default_minfreespace_)
  : _default_minfreespace(&default_minfreespace_)
{
}

int
Branch::from_string(const std::string &str_)
{
  return -EINVAL;
}

std::string
Branch::to_string(void) const
{
  std::string rv;

  rv  = path;
  rv += '=';
  switch(mode)
    {
    default:
    case Branch::Mode::RW:
      rv += "RW";
      break;
    case Branch::Mode::RO:
      rv += "RO";
      break;
    case Branch::Mode::NC:
      rv += "NC";
      break;
    }

  if(_minfreespace.has_value())
    {
      rv += ',';
      rv += num::humanize(_minfreespace.value());
    }

  return rv;
}

void
Branch::set_minfreespace(const uint64_t minfreespace_)
{
  _minfreespace = minfreespace_;
}

uint64_t
Branch::minfreespace(void) const
{
  if(_minfreespace.has_value())
    return _minfreespace.value();
  return *_default_minfreespace;
}

bool
Branch::ro(void) const
{
  return (mode == Branch::Mode::RO);
}

bool
Branch::nc(void) const
{
  return (mode == Branch::Mode::NC);
}

bool
Branch::ro_or_nc(void) const
{
  return ((mode == Branch::Mode::RO) ||
          (mode == Branch::Mode::NC));
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

    offset = s_.find_first_of('/');
    *instr_ = s_.substr(0,offset);
    if(offset != std::string::npos)
      *values_ = s_.substr(offset);
  }
}

Branches::Branches(const uint64_t &default_minfreespace_)
  : default_minfreespace(default_minfreespace_)
{
  pthread_rwlock_init(&lock,NULL);
}

namespace l
{
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
        const uint64_t &default_minfreespace_,
        BranchVec      *branches_)
  {
    int rv;
    string glob;
    vector<string> globbed;
    optional<uint64_t> minfreespace;
    Branch branch(default_minfreespace_);

    rv = l::parse_branch(str_,&glob,&branch.mode,&minfreespace);
    if(rv < 0)
      return rv;

    if(minfreespace.has_value())
      branch.set_minfreespace(minfreespace.value());

    fs::glob(glob,&globbed);
    fs::realpathize(&globbed);
    for(size_t i = 0; i < globbed.size(); i++)
      {
        branch.path = globbed[i];
        branches_->push_back(branch);
      }

    return 0;
  }

  static
  int
  set(const std::string &str_,
      Branches          *branches_)
  {
    int rv;
    vector<string> paths;
    BranchVec tmp_branchvec;

    str::split(str_,':',&paths);

    for(size_t i = 0; i < paths.size(); i++)
      {
        rv = l::parse(paths[i],branches_->default_minfreespace,&tmp_branchvec);
        if(rv < 0)
          return rv;
      }

    branches_->vec.clear();
    branches_->vec.insert(branches_->vec.end(),
                          tmp_branchvec.begin(),
                          tmp_branchvec.end());

    return 0;
  }

  static
  int
  add_begin(const std::string &str_,
            Branches          *branches_)
  {
    int rv;
    vector<string> paths;
    BranchVec tmp_branchvec;

    str::split(str_,':',&paths);

    for(size_t i = 0; i < paths.size(); i++)
      {
        rv = l::parse(paths[i],branches_->default_minfreespace,&tmp_branchvec);
        if(rv < 0)
          return rv;
      }

    branches_->vec.insert(branches_->vec.begin(),
                          tmp_branchvec.begin(),
                          tmp_branchvec.end());

    return 0;
  }

  static
  int
  add_end(const std::string &str_,
          Branches          *branches_)
  {
    int rv;
    vector<string> paths;
    BranchVec tmp_branchvec;

    str::split(str_,':',&paths);

    for(size_t i = 0; i < paths.size(); i++)
      {
        rv = l::parse(paths[i],branches_->default_minfreespace,&tmp_branchvec);
        if(rv < 0)
          return rv;
      }

    branches_->vec.insert(branches_->vec.end(),
                          tmp_branchvec.begin(),
                          tmp_branchvec.end());

    return 0;
  }

  static
  int
  erase_begin(BranchVec *branches_)
  {
    branches_->erase(branches_->begin());

    return 0;
  }

  static
  int
  erase_end(BranchVec *branches_)
  {
    branches_->pop_back();

    return 0;
  }

  static
  int
  erase_fnmatch(const std::string &str_,
                Branches          *branches_)
  {
    vector<string> patterns;

    str::split(str_,':',&patterns);

    for(BranchVec::iterator i = branches_->vec.begin();
        i != branches_->vec.end();)
      {
        int match = FNM_NOMATCH;

        for(vector<string>::const_iterator pi = patterns.begin();
            pi != patterns.end() && match != 0;
            ++pi)
          {
            match = ::fnmatch(pi->c_str(),i->path.c_str(),0);
          }

        i = ((match == 0) ? branches_->vec.erase(i) : (i+1));
      }

    return 0;
  }
}

int
Branches::from_string(const std::string &s_)
{
  rwlock::WriteGuard guard(lock);

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
    return l::erase_begin(&vec);
  if(instr == "->")
    return l::erase_end(&vec);
  if(instr == "=")
    return l::set(values,this);
  if(instr.empty())
    return l::set(values,this);

  return -EINVAL;
}

string
Branches::to_string(void) const
{
  rwlock::ReadGuard guard(lock);

  string tmp;

  for(size_t i = 0; i < vec.size(); i++)
    {
      const Branch &branch = vec[i];

      tmp += branch.to_string();
      tmp += ':';
    }

  if(*tmp.rbegin() == ':')
    tmp.erase(tmp.size() - 1);

  return tmp;
}

void
Branches::to_paths(vector<string> &vec_) const
{
  rwlock::ReadGuard guard(lock);

  for(size_t i = 0; i < vec.size(); i++)
    {
      const Branch &branch = vec[i];

      vec_.push_back(branch.path);
    }
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
  rwlock::ReadGuard guard(_branches.lock);

  std::string rv;

  for(uint64_t i = 0; i < _branches.vec.size(); i++)
    {
      rv += _branches.vec[i].path;
      rv += ':';
    }

  if(*rv.rbegin() == ':')
    rv.erase(rv.size() - 1);

  return rv;
}

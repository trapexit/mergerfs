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
#include "fs.hpp"
#include "fs_glob.hpp"
#include "str.hpp"

#include <string>

#include <errno.h>
#include <fnmatch.h>

using std::string;
using std::vector;

bool
Branch::ro(void) const
{
  return (mode == Branch::RO);
}

bool
Branch::nc(void) const
{
  return (mode == Branch::NC);
}

bool
Branch::ro_or_nc(void) const
{
  return ((mode == Branch::RO) ||
          (mode == Branch::NC));
}

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

Branches::Branches()
{
  pthread_rwlock_init(&lock,NULL);
}

static
void
parse(const string &str_,
      Branches     &branches_)
{
  string str;
  Branch branch;
  vector<string> globbed;

  str = str_;
  branch.mode = Branch::INVALID;
  if(str::endswith(str,"=RO"))
    branch.mode = Branch::RO;
  ef(str::endswith(str,"=RW"))
    branch.mode = Branch::RW;
  ef(str::endswith(str,"=NC"))
    branch.mode = Branch::NC;

  if(branch.mode != Branch::INVALID)
    str.resize(str.size() - 3);
  else
    branch.mode = Branch::RW;

  fs::glob(str,globbed);
  fs::realpathize(globbed);
  for(size_t i = 0; i < globbed.size(); i++)
    {
      branch.path = globbed[i];
      branches_.push_back(branch);
    }
}

static
void
set(Branches          &branches_,
    const std::string &str_)
{
  vector<string> paths;

  branches_.clear();

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches tmp;

      parse(paths[i],tmp);

      branches_.insert(branches_.end(),
                       tmp.begin(),
                       tmp.end());
    }
}

static
void
add_begin(Branches          &branches_,
          const std::string &str_)
{
  vector<string> paths;

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches tmp;

      parse(paths[i],tmp);

      branches_.insert(branches_.begin(),
                       tmp.begin(),
                       tmp.end());
    }
}

static
void
add_end(Branches          &branches_,
        const std::string &str_)
{
  vector<string> paths;

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches tmp;

      parse(paths[i],tmp);

      branches_.insert(branches_.end(),
                       tmp.begin(),
                       tmp.end());
    }
}

static
void
erase_begin(Branches &branches_)
{
  branches_.erase(branches_.begin());
}

static
void
erase_end(Branches &branches_)
{
  branches_.pop_back();
}

static
void
erase_fnmatch(Branches          &branches_,
              const std::string &str_)
{
  vector<string> patterns;

  str::split(patterns,str_,':');

  for(Branches::iterator i = branches_.begin();
      i != branches_.end();)
    {
      int match = FNM_NOMATCH;

      for(vector<string>::const_iterator pi = patterns.begin();
          pi != patterns.end() && match != 0;
          ++pi)
        {
          match = ::fnmatch(pi->c_str(),i->path.c_str(),0);
        }

      i = ((match == 0) ? branches_.erase(i) : (i+1));
    }
}

int
Branches::from_string(const std::string &s_)
{
  rwlock::WriteGuard guard(&lock);
  std::string instr;
  std::string values;

  ::split(s_,&instr,&values);

  if(instr == "+")
    ::add_end(*this,values);
  ef(instr == "+<")
    ::add_begin(*this,values);
  ef(instr == "+>")
    ::add_end(*this,values);
  ef(instr == "-")
    ::erase_fnmatch(*this,values);
  ef(instr == "-<")
    ::erase_begin(*this);
  ef(instr == "->")
    ::erase_end(*this);
  ef(instr == "=")
    ::set(*this,values);
  ef(instr.empty())
    ::set(*this,values);
  else
    return -EINVAL;

  return 0;
}

string
Branches::to_string(void) const
{
  rwlock::ReadGuard guard(&lock);
  string tmp;

  for(size_t i = 0; i < size(); i++)
    {
      const Branch &branch = (*this)[i];

      tmp += branch.path;

      tmp += '=';
      switch(branch.mode)
        {
        default:
        case Branch::RW:
          tmp += "RW";
          break;
        case Branch::RO:
          tmp += "RO";
          break;
        case Branch::NC:
          tmp += "NC";
          break;
        }

      tmp += ':';
    }

  if(*tmp.rbegin() == ':')
    tmp.erase(tmp.size() - 1);

  return tmp;
}

void
Branches::to_paths(vector<string> &vec_) const
{
  rwlock::ReadGuard guard(&lock);

  for(size_t i = 0; i < size(); i++)
    {
      const Branch &branch = (*this)[i];

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
  rwlock::ReadGuard guard(&_branches.lock);
  std::string rv;

  for(uint64_t i = 0; i < _branches.size(); i++)
    {
      rv += _branches[i].path;
      rv += ':';
    }

  if(*rv.rbegin() == ':')
    rv.erase(rv.size() - 1);

  return rv;
}

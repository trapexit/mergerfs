/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "str.hpp"

#include <cassert>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fnmatch.h>

using std::istringstream;
using std::set;
using std::string;
using std::string_view;
using std::vector;


void
str::split(const string_view  str_,
           const char         delimiter_,
           vector<string>    *result_)
{
  size_t pos;
  size_t start;
  size_t length;

  assert(result_ != nullptr);
  if(str_.empty())
    return;

  start = 0;
  pos   = str_.find(delimiter_,start);
  while(pos != std::string_view::npos)
    {
      length = (pos - start);
      result_->push_back(std::string{str_.substr(start,length)});
      start = pos + 1;
      pos = str_.find(delimiter_,start);
    }

  result_->push_back(std::string{str_.substr(start)});
}

void
str::split(const string_view  str_,
           const char         delimiter_,
           set<string>       *result_)
{
  size_t pos;
  size_t start;
  size_t length;

  assert(result_ != nullptr);
  if(str_.empty())
    return;

  start = 0;
  pos   = str_.find(delimiter_,start);
  while(pos != std::string_view::npos)
    {
      length = (pos - start);
      result_->insert(std::string{str_.substr(start,length)});
      start = pos + 1;
      pos = str_.find(delimiter_,start);
    }

  result_->insert(std::string{str_.substr(start)});
}

void
str::split_on_null(const std::string_view    str_,
                   std::vector<std::string> *result_)
{
  return str::split(str_,'\0',result_);
}

void
str::lsplit1(const string_view &str_,
             const char         delimiter_,
             vector<string>    *result_)
{
  std::size_t off;

  assert(result_ != nullptr);
  if(str_.empty())
    return;

  off = str_.find(delimiter_);
  if(off == std::string::npos)
    {
      result_->push_back(std::string{str_});
    }
  else
    {
      result_->push_back(std::string{str_.substr(0,off)});
      result_->push_back(std::string{str_.substr(off+1)});
    }
}

void
str::rsplit1(const string_view &str_,
             const char         delimiter_,
             vector<string>    *result_)
{
  std::size_t off;

  assert(result_ != nullptr);
  if(str_.empty())
    return;

  off = str_.rfind(delimiter_);
  if(off == std::string::npos)
    {
      result_->push_back(std::string{str_});
    }
  else
    {
      result_->push_back(std::string{str_.substr(0,off)});
      result_->push_back(std::string{str_.substr(off+1)});
    }
}

void
str::splitkv(const std::string_view  str_,
             const char              delimiter_,
             std::string            *key_,
             std::string            *val_)
{
  size_t pos;

  pos = str_.find(delimiter_);
  if(pos != std::string_view::npos)
    {
      *key_ = str_.substr(0, pos);
      *val_ = str_.substr(pos + 1);
    }
  else
    {
      *key_ = str_;
      *val_ = "";
    }
}

string
str::join(const vector<string> &vec_,
          const size_t          substridx_,
          const char            sep_)
{
  if(vec_.empty())
    return string();

  string rv = vec_[0].substr(substridx_);
  for(size_t i = 1; i < vec_.size(); i++)
    rv += sep_ + vec_[i].substr(substridx_);

  return rv;
}

string
str::join(const vector<string> &vec_,
          const char            sep_)
{
  return str::join(vec_,0,sep_);
}

string
str::join(const set<string> &set_,
          const char         sep_)
{
  string rv;

  for(auto const &s : set_)
    rv += s + sep_;
  rv.pop_back();

  return rv;
}

size_t
str::longest_common_prefix_index(const vector<string> &vec_)
{
  if(vec_.empty())
    return string::npos;

  for(size_t n = 0; n < vec_[0].size(); n++)
    {
      char chr = vec_[0][n];
      for(size_t i = 1; i < vec_.size(); i++)
        {
          if(n >= vec_[i].size())
            return n;
          if(vec_[i][n] != chr)
            return n;
        }
    }

  return string::npos;
}

string
str::longest_common_prefix(const vector<string> &vec_)
{
  size_t idx;

  idx = longest_common_prefix_index(vec_);
  if(idx != string::npos)
    return vec_[0].substr(0,idx);

  return string();
}

string
str::remove_common_prefix_and_join(const vector<string> &vec_,
                              const char            sep_)
{
  size_t idx;

  idx = str::longest_common_prefix_index(vec_);
  if(idx == string::npos)
    idx = 0;

  return str::join(vec_,idx,sep_);
}

void
str::erase_fnmatches(const vector<string> &patterns_,
                     vector<string>       &strs_)
{
  vector<string>::iterator si;
  vector<string>::const_iterator pi;

  si = strs_.begin();
  while(si != strs_.end())
    {
      int match = FNM_NOMATCH;

      for(pi = patterns_.begin();
          pi != patterns_.end() && match != 0;
          ++pi)
        {
          match = fnmatch(pi->c_str(),si->c_str(),0);
        }

      if(match == 0)
        si = strs_.erase(si);
      else
        ++si;
    }
}

bool
str::isprefix(const string &s0_,
              const string &s1_)
{
  return ((s0_.size() >= s1_.size()) &&
          (s0_.compare(0,s1_.size(),s1_) == 0));
}

bool
str::startswith(const string &str_,
                const string &prefix_)
{
  return ((str_.size() >= prefix_.size()) &&
          (str_.compare(0,prefix_.size(),prefix_) == 0));
}

bool
str::endswith(const string &str_,
              const string &suffix_)
{
  if(suffix_.size() > str_.size())
    return false;

  return std::equal(suffix_.rbegin(),
                    suffix_.rend(),
                    str_.rbegin());
}

std::string
str::trim(const std::string &str_)
{
  std::string rv;

  rv = str_;

  while(!rv.empty() && (rv[0] == ' '))
    rv.erase(0);
  while(!rv.empty() && (rv[rv.size()-1] == ' '))
    rv.erase(rv.size()-1);

  return rv;
}

bool
str::eq(const char *s0_,
        const char *s1_)
{
  return (strcmp(s0_,s1_) == 0);
}

bool
str::startswith(const char *s_,
                const char *p_)
{
  while(*p_)
    {
      if(*p_ != *s_)
        return false;

      p_++;
      s_++;
    }

  return true;
}

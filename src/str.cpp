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

#include "str.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fnmatch.h>

using std::set;
using std::string;
using std::string_view;
using std::vector;


std::vector<std::string>
str::split(const string_view str_,
           const char        delimiter_)
{
  std::vector<std::string> result;

  if(str_.empty())
    return result;

  size_t start = 0;
  size_t pos = str_.find(delimiter_, start);
  while(pos != std::string_view::npos)
    {
      size_t length = (pos - start);
      result.emplace_back(str_.substr(start, length));
      start = pos + 1;
      pos = str_.find(delimiter_, start);
    }

  result.emplace_back(str_.substr(start));

  return result;
}

std::set<std::string>
str::split_to_set(const string_view str_,
                  const char        delimiter_)
{
  std::set<std::string> result;

  if(str_.empty())
    return result;

  size_t start = 0;
  size_t pos = str_.find(delimiter_, start);
  while(pos != std::string_view::npos)
    {
      size_t length = (pos - start);
      result.emplace(str_.substr(start, length));
      start = pos + 1;
      pos = str_.find(delimiter_, start);
    }

  result.emplace(str_.substr(start));

  return result;
}

std::vector<std::string>
str::split_on_null(const std::string_view str_)
{
  return str::split(str_, '\0');
}

std::vector<std::string>
str::lsplit1(const string_view str_,
             const char        delimiter_)
{
  std::vector<std::string> result;

  if(str_.empty())
    return result;

  std::size_t off = str_.find(delimiter_);
  if(off == std::string_view::npos)
    {
      result.emplace_back(str_);
    }
  else
    {
      result.emplace_back(str_.substr(0, off));
      result.emplace_back(str_.substr(off + 1));
    }

  return result;
}

std::vector<std::string>
str::rsplit1(const string_view str_,
             const char        delimiter_)
{
  std::vector<std::string> result;

  if(str_.empty())
    return result;

  std::size_t off = str_.rfind(delimiter_);
  if(off == std::string_view::npos)
    {
      result.emplace_back(str_);
    }
  else
    {
      result.emplace_back(str_.substr(0, off));
      result.emplace_back(str_.substr(off + 1));
    }

  return result;
}

std::pair<std::string, std::string>
str::splitkv(const std::string_view str_,
             const char             delimiter_)
{
  std::size_t pos = str_.find(delimiter_);
  if(pos != std::string_view::npos)
    return {std::string{str_.substr(0, pos)},
            std::string{str_.substr(pos + 1)}};

  return {std::string{str_}, std::string{}};
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
  return str::join(vec_, 0, sep_);
}

string
str::join(const set<string> &set_,
          const char         sep_)
{
  string rv;

  if(set_.empty())
    return {};

  for(auto const &s : set_)
    rv += s + sep_;

  if(!rv.empty())
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
  size_t idx = longest_common_prefix_index(vec_);
  if(idx != string::npos)
    return vec_[0].substr(0, idx);

  return string();
}

string
str::remove_common_prefix_and_join(const vector<string> &vec_,
                                   const char            sep_)
{
  size_t idx = str::longest_common_prefix_index(vec_);
  if(idx == string::npos)
    idx = 0;

  return str::join(vec_, idx, sep_);
}

void
str::erase_fnmatches(const vector<string> &patterns_,
                     vector<string>       &strs_)
{
  auto si = strs_.begin();
  while(si != strs_.end())
    {
      int match = FNM_NOMATCH;

      for(auto pi = patterns_.begin();
          pi != patterns_.end() && match != 0;
          ++pi)
        {
          match = fnmatch(pi->c_str(), si->c_str(), 0);
        }

      if(match == 0)
        si = strs_.erase(si);
      else
        ++si;
    }
}

bool
str::startswith(const string_view str_,
                const string_view prefix_) noexcept
{
  if(prefix_.size() > str_.size())
    return false;
  return str_.compare(0, prefix_.size(), prefix_) == 0;
}

bool
str::endswith(const string_view str_,
              const string_view suffix_) noexcept
{
  if(suffix_.size() > str_.size())
    return false;
  return str_.compare(str_.size() - suffix_.size(),
                      suffix_.size(),
                      suffix_) == 0;
}

std::string
str::trim(const std::string_view str_)
{
  if(str_.empty())
    return std::string{};

  auto start = str_.begin();
  auto end = str_.end();

  while(start != end && std::isspace(static_cast<unsigned char>(*start)))
    ++start;

  if(start == end)
    return std::string{};

  --end;
  while(end != start && std::isspace(static_cast<unsigned char>(*end)))
    --end;

  return std::string{start, end + 1};
}

bool
str::eq(const char *s0_,
        const char *s1_) noexcept
{
  return (std::strcmp(s0_, s1_) == 0);
}

bool
str::startswith(const char *s_,
                const char *p_) noexcept
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

std::string
str::replace(const std::string_view str_,
             const char src_,
             const char dst_)
{
  std::string result;
  result.reserve(str_.size());

  for(char c : str_)
    result.push_back(c == src_ ? dst_ : c);

  return result;
}

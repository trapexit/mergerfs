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

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <fnmatch.h>

using std::istringstream;
using std::set;
using std::string;
using std::vector;

namespace str
{
  void
  split(const char     *str_,
        const char      delimiter_,
        vector<string> *result_)
  {
    string part;
    istringstream ss(str_);

    while(std::getline(ss,part,delimiter_))
      result_->push_back(part);
  }

  void
  split(const string   &str_,
        const char      delimiter_,
        vector<string> *result_)
  {
    return str::split(str_.c_str(),delimiter_,result_);
  }

  void
  rsplit1(const string   &str_,
          const char      delimiter_,
          vector<string> *result_)
  {
    std::size_t off;

    off = str_.rfind('=');
    if(off == std::string::npos)
      {
        result_->push_back(str_);
      }
    else
      {
        result_->push_back(str_.substr(0,off));
        result_->push_back(str_.substr(off+1));
      }
  }

  void
  splitkv(const string &str_,
          const char    delimiter_,
          string       *key_,
          string       *val_)
  {
    istringstream iss;
    std::string key;
    std::string val;

    iss.str(str_);
    std::getline(iss,key,delimiter_);
    std::getline(iss,val,'\0');

    *key_ = key;
    *val_ = val;
  }

  string
  join(const vector<string> &vec_,
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
  join(const vector<string> &vec_,
       const char            sep_)
  {
    return str::join(vec_,0,sep_);
  }

  string
  join(const set<string> &set_,
       const char         sep_)
  {
    string rv;
    set<string>::iterator i;

    if(set_.empty())
      return string();

    i = set_.begin();
    rv += *i;
    ++i;
    for(; i != set_.end(); ++i)
      rv += sep_ + *i;

    return rv;
  }

  size_t
  longest_common_prefix_index(const vector<string> &vec_)
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
  longest_common_prefix(const vector<string> &vec_)
  {
    size_t idx;

    idx = longest_common_prefix_index(vec_);
    if(idx != string::npos)
      return vec_[0].substr(0,idx);

    return string();
  }

  string
  remove_common_prefix_and_join(const vector<string> &vec_,
                                const char            sep_)
  {
    size_t idx;

    idx = str::longest_common_prefix_index(vec_);
    if(idx == string::npos)
      idx = 0;

    return str::join(vec_,idx,sep_);
  }

  void
  erase_fnmatches(const vector<string> &patterns_,
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
  isprefix(const string &s0_,
           const string &s1_)
  {
    return ((s0_.size() >= s1_.size()) &&
            (s0_.compare(0,s1_.size(),s1_) == 0));
  }

  bool
  startswith(const string &str_,
             const string &prefix_)
  {
    return ((str_.size() >= prefix_.size()) &&
            (str_.compare(0,prefix_.size(),prefix_) == 0));
  }

  bool
  endswith(const string &str_,
           const string &suffix_)
  {
    if(suffix_.size() > str_.size())
      return false;

    return std::equal(suffix_.rbegin(),
                      suffix_.rend(),
                      str_.rbegin());
  }

  std::string
  trim(const std::string &str_)
  {
    std::string rv;

    rv = str_;

    while(!rv.empty() && (rv[0] == ' '))
      rv.erase(0);
    while(!rv.empty() && (rv[rv.size()-1] == ' '))
      rv.erase(rv.size()-1);

    return rv;
  }
}

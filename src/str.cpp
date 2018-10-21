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

#include <string>
#include <vector>
#include <sstream>

#include <fnmatch.h>

using std::string;
using std::vector;
using std::istringstream;

namespace str
{
  void
  split(vector<string> &result,
        const char     *str,
        const char      delimiter)
  {
    string part;
    istringstream ss(str);

    while(std::getline(ss,part,delimiter))
      result.push_back(part);
  }

  void
  split(vector<string> &result,
        const string   &str,
        const char      delimiter)
  {
    return split(result,str.c_str(),delimiter);
  }

  string
  join(const vector<string> &vec,
       const size_t          substridx,
       const char            sep)
  {
    if(vec.empty())
      return string();

    string rv = vec[0].substr(substridx);
    for(size_t i = 1; i < vec.size(); i++)
      rv += sep + vec[i].substr(substridx);

    return rv;
  }

  string
  join(const vector<string> &vec,
       const char            sep)
  {
    return str::join(vec,0,sep);
  }

  size_t
  longest_common_prefix_index(const vector<string> &vec)
  {
    if(vec.empty())
      return string::npos;

    for(size_t n = 0; n < vec[0].size(); n++)
      {
        char chr = vec[0][n];
        for(size_t i = 1; i < vec.size(); i++)
          {
            if(n >= vec[i].size())
              return n;
            if(vec[i][n] != chr)
              return n;
          }
      }

    return string::npos;
  }

  string
  longest_common_prefix(const vector<string> &vec)
  {
    size_t idx;

    idx = longest_common_prefix_index(vec);
    if(idx != string::npos)
      return vec[0].substr(0,idx);

    return string();
  }

  string
  remove_common_prefix_and_join(const vector<string> &vec,
                                const char            sep)
  {
    size_t idx;

    idx = str::longest_common_prefix_index(vec);
    if(idx == string::npos)
      idx = 0;

    return str::join(vec,idx,sep);
  }

  void
  erase_fnmatches(const vector<string> &patterns,
                  vector<string>       &strs)
  {
    vector<string>::iterator si;
    vector<string>::const_iterator pi;

    si = strs.begin();
    while(si != strs.end())
      {
        int match = FNM_NOMATCH;

        for(pi = patterns.begin();
            pi != patterns.end() && match != 0;
            ++pi)
          {
            match = fnmatch(pi->c_str(),si->c_str(),0);
          }

        if(match == 0)
          si = strs.erase(si);
        else
          ++si;
      }
  }

  bool
  isprefix(const string &s0,
           const string &s1)
  {
    return ((s0.size() >= s1.size()) &&
            (s0.compare(0,s1.size(),s1) == 0));
  }

  bool
  ends_with(const string &str_,
            const string &suffix_)
  {
    if(suffix_.size() > str_.size())
      return false;

    return std::equal(suffix_.rbegin(),
                      suffix_.rend(),
                      str_.rbegin());
  }
}

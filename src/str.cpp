/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
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
        const string   &str,
        const char      delimiter)
  {
    string part;
    istringstream ss(str);

    while(std::getline(ss,part,delimiter))
      result.push_back(part);
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
}

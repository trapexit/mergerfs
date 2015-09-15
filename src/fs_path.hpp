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

#ifndef __FS_PATH_HPP__
#define __FS_PATH_HPP__

#include <string>
#include <vector>

namespace fs
{
  namespace path
  {
    using std::string;
    using std::vector;

    string dirname(const string &path);
    string basename(const string &path);

    bool is_empty(const string &path);

    bool exists(vector<string>::const_iterator  begin,
                vector<string>::const_iterator  end,
                const string                   &fusepath);
    bool exists(const vector<string>           &srcmounts,
                const string                   &fusepath);

    inline
    string
    make(const string &base,
         const string &suffix)
    {
      return base + suffix;
    }

    inline
    void
    make(const string &base,
         const string &suffix,
         string       &output)
    {
      output = base + suffix;
    }

    inline
    void
    append(string       &base,
           const string &suffix)
    {
      base += suffix;
    }
  }
};

#endif // __FS_PATH_HPP__

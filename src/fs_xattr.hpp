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

#ifndef __FS_XATTR_HPP__
#define __FS_XATTR_HPP__

#include <string>
#include <vector>
#include <map>

namespace fs
{
  namespace xattr
  {
    using std::size_t;
    using std::string;
    using std::vector;
    using std::map;

  
    int list(const string   &path,
             vector<char>   &attrs);
    int list(const string   &path,
             string         &attrs);
    int list(const string   &path,
             vector<string> &attrs);

    int get(const string &path,
            const string &attr,
            vector<char> &value);
    int get(const string &path,
            const string &attr,
            string       &value);

    int get(const string       &path,
            map<string,string> &attrs);

    int set(const string &path,
            const string &key,
            const string &value,
            const int     flags);
    int set(const int     fd,
            const string &key,
            const string &value,
            const int     flags);

    int set(const string             &path,
            const map<string,string> &attrs);

    int copy(const int fdin,
             const int fdout);
    int copy(const string &from,
             const string &to);
  }
}

#endif // __FS_HPP__

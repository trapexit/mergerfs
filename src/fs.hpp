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

#ifndef __FS_HPP__
#define __FS_HPP__

#include <string>
#include <vector>
#include <map>

namespace fs
{
  using std::string;
  using std::vector;
  using std::map;

  struct Path
  {
    Path() {}
    Path(const string b,
         const string f)
      : base(b),
        full(f)
    {}

    string base;
    string full;
  };

  typedef vector<Path> PathVector;
  typedef void (*SearchFunc)(const vector<string>&,const string,PathVector&);

  string dirname(const string path);
  string basename(const string path);

  bool dir_is_empty(const string path);

  string make_path(const string base,
                   const string suffix);

  bool path_exists(vector<string>::const_iterator  begin,
                   vector<string>::const_iterator  end,
                   const string                    fusepath);
  bool path_exists(const vector<string>           &srcmounts,
                   const string                    fusepath);

  int clonepath(const string srcfrom,
                const string srcto,
                const string relative);

  int listxattr(const string    path,
                vector<char>   &attrs);
  int listxattr(const string    path,
                string         &attrs);
  int listxattr(const string    path,
                vector<string> &attrs);

  int getxattr(const string  path,
               const string  attr,
               vector<char> &value);
  int getxattr(const string  path,
               const string  attr,
               string       &value);

  int getxattrs(const string        path,
                map<string,string> &attrs);

  int setxattr(const string path,
               const string key,
               const string value,
               const int    flags);
  int setxattr(const int    fd,
               const string key,
               const string value,
               const int    flags);

  int setxattrs(const string              path,
                const map<string,string> &attrs);

  int copyxattrs(const string from,
                 const string to);

  int copyattr(const string from,
               const string to);

  void glob(const vector<string> &patterns,
            vector<string>       &strs);

  void erase_fnmatches(const vector<string> &patterns,
                       vector<string>       &strs);

  namespace find
  {
    void invalid(const vector<string> &basepaths,
                 const string          fusepath,
                 PathVector           &paths);
    void ff(const vector<string>      &basepaths,
            const string               fusepath,
            PathVector                &paths);
    void ffwp(const vector<string>    &paths,
              const string             fusepath,
              PathVector              &rv);
    void newest(const vector<string>  &paths,
                const string           fusepath,
                PathVector            &rv);
    void all(const vector<string>     &paths,
             const string              fusepath,
             PathVector               &rv);
    void mfs(const vector<string>     &paths,
             const string              fusepath,
             PathVector               &rv);
    void epmfs(const vector<string>   &paths,
               const string            fusepath,
               PathVector             &rv);
    void rand(const vector<string>    &paths,
              const string             fusepath,
              PathVector              &rv);
  }
};

#endif // __FS_HPP__

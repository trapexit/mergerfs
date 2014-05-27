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
#include <map>
#include <sstream>
#include <cstdlib>
#include <iterator>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "fs.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using std::map;
using std::istringstream;

template<typename Container>
Container&
split(Container                                  &result,
      const typename Container::value_type       &s,
      typename Container::value_type::value_type  delimiter)
{
  string        str;
  istringstream ss(s);

  while(std::getline(ss,str,delimiter))
    result.push_back(str);

  return result;
}

template <typename Iter>
Iter
random_element(Iter begin,
               Iter end)
{
  const unsigned long n = std::distance(begin, end);

  std::advance(begin, (std::rand() % n));

  return begin;
}

namespace fs
{
  string
  dirname(const string path)
  {
    string parent = path;
    string::reverse_iterator i;
    string::reverse_iterator bi;

    bi = parent.rend();
    i  = parent.rbegin();
    while(*i == '/' && i != bi)
      i++;

    while(*i != '/' && i != bi)
      i++;

    while(*i == '/' && i != bi)
      i++;

    parent.erase(i.base(),parent.end());

    return parent;
  }

  string
  basename(const string path)
  {
    return path.substr(path.find_last_of('/')+1);
  }

  bool
  dir_is_empty(const string path)
  {
    DIR           *dir;
    struct dirent *de;

    dir = ::opendir(path.c_str());
    if(!dir)
      return false;

    while((de = ::readdir(dir)))
      {
        const char *d_name = de->d_name;

        if(d_name[0] == '.'     &&
           ((d_name[1] == '\0') ||
            (d_name[1] == '.' && d_name[2] == '\0')))
          continue;

        ::closedir(dir);

        return false;
      }

    ::closedir(dir);

    return true;
  }

  string
  make_path(const string base,
            const string suffix)
  {
    if(*base.rbegin() == '/' ||
       *suffix.rbegin() == '/')
      return base + suffix;
    return base + '/' + suffix;
  }

  bool
  path_exists(vector<string>::const_iterator begin,
              vector<string>::const_iterator end,
              const string                   fusepath)
  {
    for(vector<string>::const_iterator
          iter = begin; iter != end; ++iter)
      {
        int         rv;
        struct stat st;
        string      path;

        path = make_path(*iter,fusepath);
        rv   = ::lstat(path.c_str(),&st);
        if(rv == 0)
          return true;
      }

    return false;
  }

  bool
  path_exists(const vector<string> &srcmounts,
              const string               fusepath)
  {
    return path_exists(srcmounts.begin(),
                       srcmounts.end(),
                       fusepath);
  }

  Path
  find_first(vector<string>::const_iterator beginiter,
             vector<string>::const_iterator enditer,
             const string                   suffix)
  {
    for(vector<string>::const_iterator
          iter = beginiter; iter != enditer; ++iter)
      {
        int         rv;
        struct stat st;
        string      path;

        path = fs::make_path(*iter,suffix);
        rv   = ::lstat(path.c_str(),&st);
        if(rv == 0 || (rv == -1 && errno == EACCES))
          return Path(*iter,path);
      }

    return Path();
  }

  Path
  find_first(const vector<string> &basepaths,
             const string          suffix)
  {
    return find_first(basepaths.begin(),
                      basepaths.end(),
                      suffix);
  }

  void
  find_first(const vector<string> &basepaths,
             const string          suffix,
             vector<Path>         &paths)
  {
    paths.push_back(find_first(basepaths,suffix));
  }

  Path
  find_first_with_permission(vector<string>::const_iterator beginiter,
                             vector<string>::const_iterator enditer,
                             const string                   suffix)
  {
    for(vector<string>::const_iterator
          iter = beginiter; iter != enditer; ++iter)
      {
        int         rv;
        struct stat st;
        string      path;

        path = make_path(*iter,suffix);
        rv   = ::lstat(path.c_str(),&st);
        if(rv == 0)
          return Path(*iter,path);
      }

    return Path();
  }

  Path
  find_first_with_permission(const vector<string> &basepaths,
                             const string          suffix)
  {
    return find_first_with_permission(basepaths.begin(),
                                      basepaths.end(),
                                      suffix);
  }

  void
  find_first_with_permission(const vector<string> &basepaths,
                             const string          suffix,
                             vector<Path>         &paths)
  {
    paths.push_back(find_first_with_permission(basepaths,suffix));
  }

  Path
  find_newest(vector<string>::const_iterator beginiter,
              vector<string>::const_iterator enditer,
              const string                   suffix)
  {
    time_t newest;
    string npath;
    vector<string>::const_iterator niter;

    newest = 0;
    for(vector<string>::const_iterator
          iter = beginiter; iter != enditer; ++iter)
      {
        int rv;
        struct stat st;
        const string path = make_path(*iter,suffix);

        rv  = ::lstat(path.c_str(),&st);
        if(rv == 0 && st.st_mtime > newest)
          {
            newest = st.st_mtime;
            niter  = iter;
            npath  = path;
          }
      }

    if(newest)
      return Path(*niter,npath);

    return Path();
  }

  Path
  find_newest(const vector<string> &basepaths,
              const string          suffix)
  {
    return find_newest(basepaths.begin(),
                       basepaths.end(),
                       suffix);
  }

  void
  find_newest(const vector<string> &basepaths,
              const string          suffix,
              vector<Path>         &paths)
  {
    paths.push_back(find_newest(basepaths,suffix));
  }

  void
  find_all(const vector<string> &basepaths,
           const string          suffix,
           vector<Path>         &paths)
  {
    for(vector<string>::const_iterator
          i = basepaths.begin(), ei = basepaths.end(); i != ei; ++i)
      {
        int         rv;
        struct stat st;
        string      path;

        path = fs::make_path(*i,suffix);
        rv   = ::lstat(path.c_str(),&st);
        if(rv == 0 || (rv == -1 && errno == EACCES))
          paths.push_back(Path(*i,path));
      }
  }

  Path
  find_mfs(vector<string>::const_iterator iter,
           vector<string>::const_iterator enditer,
           const string                   fusepath)
  {
    fsblkcnt_t mfs = 0;
    string mfspath;
    string fullmfspath;

    for(;iter != enditer; ++iter)
      {
        int rv;
        struct statvfs fsstats;
        const string   mountpoint = *iter;

        rv = ::statvfs(mountpoint.c_str(),&fsstats);
        if(rv == 0)
          {
            fsblkcnt_t  spaceavail;

            spaceavail = (fsstats.f_bsize * fsstats.f_bavail);
            if(spaceavail > mfs)
              {
                mfs     = spaceavail;
                mfspath = mountpoint;
              }
          }
      }

    fullmfspath = make_path(mfspath,fusepath);

    return Path(mfspath,fullmfspath);
  }

  Path
  find_mfs(const vector<string> &paths,
           const string          fusepath)
  {
    return find_mfs(paths.begin(),
                    paths.end(),
                    fusepath);
  }

  Path
  find_mfs_existing(vector<string>::const_iterator iter,
                    vector<string>::const_iterator enditer,
                    const string                   fusepath)
  {
    fsblkcnt_t existingmfs = 0;
    fsblkcnt_t generalmfs  = 0;
    string     path;
    string     generalmfspath;
    string     existingmfspath;

    do
      {
        int rv;
        struct statvfs fsstats;
        const string   mountpoint = *iter;

        rv = ::statvfs(mountpoint.c_str(),&fsstats);
        if(rv == 0)
          {
            struct stat filestats;
            fsblkcnt_t  spaceavail;

            spaceavail = (fsstats.f_bsize * fsstats.f_bavail);
            if(spaceavail > generalmfs)
              {
                generalmfs     = spaceavail;
                generalmfspath = mountpoint;
              }

            path = make_path(mountpoint,fusepath);
            rv = ::lstat(path.c_str(),&filestats);
            if(rv == 0)
              {
                if(spaceavail > existingmfs)
                  {
                    existingmfs     = spaceavail;
                    existingmfspath = mountpoint;
                  }
              }
          }

        ++iter;
      }
    while(iter != enditer);

    if(existingmfspath.empty())
      existingmfspath = generalmfspath;

    return Path(existingmfspath,path);
  }

  Path
  find_mfs_existing(const vector<string> &paths,
                    const string          fusepath)
  {
    return find_mfs_existing(paths.begin(),
                             paths.end(),
                             fusepath);
  }

  Path
  find_random(vector<string>::const_iterator iter,
              vector<string>::const_iterator enditer,
              string                         fusepath)
  {
    string randombasepath;
    string randomfullpath;

    randombasepath = *random_element(iter,enditer);
    randomfullpath = make_path(randombasepath,fusepath);

    return Path(randombasepath,randomfullpath);
  }

  Path
  find_random(const vector<string> &paths,
              const string          fusepath)
  {
    return find_random(paths.begin(),
                       paths.end(),
                       fusepath);
  }

  int
  listxattr(const string  path,
            vector<char> &attrs)
  {
#ifndef WITHOUT_XATTR
    int rv;
    int size;

    rv    = -1;
    errno = ERANGE;
    while(rv == -1 && errno == ERANGE)
      {
        size = ::listxattr(path.c_str(),NULL,0);
        attrs.resize(size);
        rv   = ::listxattr(path.c_str(),&attrs[0],size);
      }

    return rv;
#else
    errno = ENOTSUP;
    return -1;
#endif
  }

  int
  listxattr(const string         path,
            vector<string> &attrvector)
  {
    int rv;
    vector<char> attrs;

    rv = listxattr(path,attrs);
    if(rv != -1)
      {
        string tmp(attrs.begin(),attrs.end());
        split(attrvector,tmp,'\0');
      }

    return rv;
  }

  int
  listxattr(const string  path,
            string &attrstr)
  {
    int rv;
    vector<char> attrs;

    rv = listxattr(path,attrs);
    if(rv != -1)
      attrstr = string(attrs.begin(),attrs.end());

    return rv;
  }

  int
  getxattr(const string  path,
           const string  attr,
           vector<char> &value)
  {
#ifndef WITHOUT_XATTR
    int rv;
    int size;

    rv    = -1;
    errno = ERANGE;
    while(rv == -1 && errno == ERANGE)
      {
        size = ::getxattr(path.c_str(),attr.c_str(),NULL,0);
        value.resize(size);
        rv   = ::getxattr(path.c_str(),attr.c_str(),&value[0],size);
      }

    return rv;
#else
    errno = ENOTSUP;
    return -1;
#endif
  }

  int
  getxattr(const string  path,
           const string  attr,
           string       &value)
  {
    int          rv;
    vector<char> tmpvalue;

    rv = getxattr(path,attr,tmpvalue);
    if(rv != -1)
      value = string(tmpvalue.begin(),tmpvalue.end());

    return rv;
  }


  int
  getxattrs(const string        path,
            map<string,string> &attrs)
  {
    int rv;
    string attrstr;

    rv = listxattr(path,attrstr);
    if(rv == -1)
      return -1;

    {
      string key;
      istringstream ss(attrstr);

      while(getline(ss,key,'\0'))
        {
          string value;

          rv = getxattr(path,key,value);
          if(rv != -1)
            attrs[key] = value;
        }
    }

    return 0;
  }

  int
  setxattr(const string path,
           const string key,
           const string value,
           const int    flags)
  {
#ifndef WITHOUT_XATTR
    return ::setxattr(path.c_str(),
                      key.c_str(),
                      value.data(),
                      value.size(),
                      flags);
#else
    errno = ENOTSUP;
    return -1;
#endif
  }

  int
  setxattr(const int    fd,
           const string key,
           const string value,
           const int    flags)
  {
#ifndef WITHOUT_XATTR
    return ::fsetxattr(fd,
                       key.c_str(),
                       value.data(),
                       value.size(),
                       flags);
#else
    errno = ENOTSUP;
    return -1;
#endif
  }

  int
  setxattrs(const string              path,
            const map<string,string> &attrs)
  {
    int fd;

    fd = ::open(path.c_str(),O_RDONLY|O_NONBLOCK);
    if(fd == -1)
      return -1;

    for(map<string,string>::const_iterator
          i = attrs.begin(), ei = attrs.end(); i != ei; ++i)
      {
        setxattr(fd,i->first,i->second,0);
      }

    return ::close(fd);
  }

  int
  copyxattrs(const string from,
             const string to)
  {
    int rv;
    map<string,string> attrs;

    rv = getxattrs(from,attrs);
    if(rv == -1)
      return -1;

    return setxattrs(to,attrs);
  }

  int
  copyattr(const string from,
           const string to)
  {
    int fd;
    int rv;
    int flags;
    int error;
    const int openflags = O_RDONLY|O_NONBLOCK;

    fd = ::open(from.c_str(),openflags);
    if(fd == -1)
      return -1;

    rv = ::ioctl(fd,FS_IOC_GETFLAGS,&flags);
    if(rv == -1)
      {
        error = errno;
        ::close(fd);
        errno = error;
        return -1;
      }

    fd = ::open(to.c_str(),openflags);
    if(fd == -1)
      return -1;

    rv = ::ioctl(fd,FS_IOC_SETFLAGS,&flags);
    if(rv == -1)
      {
        error = errno;
        ::close(fd);
        errno = error;
        return -1;
      }

    return ::close(fd);
  }

  int
  clonepath(const string fromsrc,
            const string tosrc,
            const string relative)
  {
    int         rv;
    struct stat st;
    string      topath;
    string      frompath;
    string      dirname;

    dirname = fs::dirname(relative);
    if(!dirname.empty())
      {
        rv = clonepath(fromsrc,tosrc,dirname);
        if(rv == -1)
          return -1;
      }

    frompath = make_path(fromsrc,relative);

    rv = ::stat(frompath.c_str(),&st);
    if(rv == -1)
      return -1;
    else if(!S_ISDIR(st.st_mode))
      return (errno = ENOTDIR,-1);

    topath = make_path(tosrc,relative);
    rv = ::mkdir(topath.c_str(),st.st_mode);
    if(rv == -1)
      return -1;

    rv = ::chown(topath.c_str(),st.st_uid,st.st_gid);
    if(rv == -1)
      return -1;

    // It may not support it... it's fine...
    rv = copyattr(frompath,topath);
    if(rv == -1 && errno != ENOTTY)
      return -1;

    rv = copyxattrs(frompath,topath);
    if(rv == -1 && errno != ENOTTY)
      return -1;

    return (errno = 0);
  }
};

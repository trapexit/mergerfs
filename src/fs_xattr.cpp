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

#include <stdlib.h>
#include <sys/types.h>
#ifndef WITHOUT_XATTR
#include <attr/xattr.h>
#endif
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "str.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using std::map;
using std::istringstream;

namespace fs
{
  namespace xattr
  {
    int
    list(const int     fd,
         vector<char> &attrs)
    {
#ifndef WITHOUT_XATTR
      ssize_t rv;
      ssize_t size;

      rv    = -1;
      errno = ERANGE;
      while(rv == -1 && errno == ERANGE)
        {
          size = ::flistxattr(fd,NULL,0);
          attrs.resize(size);
          if(size == 0)
            return 0;
          rv = ::flistxattr(fd,&attrs[0],size);
        }

      return rv;
#else
      errno = ENOTSUP;
      return -1;
#endif
    }

    int
    list(const string &path,
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
          if(size == 0)
            return 0;
          rv = ::listxattr(path.c_str(),&attrs[0],size);
        }

      return rv;
#else
      errno = ENOTSUP;
      return -1;
#endif
    }

    int
    list(const int        fd,
          vector<string> &attrvector)
    {
      int rv;
      vector<char> attrs;

      rv = list(fd,attrs);
      if(rv != -1)
        {
          string tmp(attrs.begin(),attrs.end());
          str::split(attrvector,tmp,'\0');
        }

      return rv;
    }

    int
    list(const string   &path,
         vector<string> &attrvector)
    {
      int rv;
      vector<char> attrs;

      rv = list(path,attrs);
      if(rv != -1)
        {
          string tmp(attrs.begin(),attrs.end());
          str::split(attrvector,tmp,'\0');
        }

      return rv;
    }

    int
    list(const int  fd,
          string   &attrstr)
    {
      int rv;
      vector<char> attrs;

      rv = list(fd,attrs);
      if(rv != -1)
        attrstr = string(attrs.begin(),attrs.end());

      return rv;
    }

    int
    list(const string &path,
         string       &attrstr)
    {
      int rv;
      vector<char> attrs;

      rv = list(path,attrs);
      if(rv != -1)
        attrstr = string(attrs.begin(),attrs.end());

      return rv;
    }

    int
    get(const int     fd,
        const string &attr,
        vector<char> &value)
    {
#ifndef WITHOUT_XATTR
      int rv;
      int size;

      rv    = -1;
      errno = ERANGE;
      while(rv == -1 && errno == ERANGE)
        {
          size = ::fgetxattr(fd,attr.c_str(),NULL,0);
          value.resize(size);
          if(size == 0)
            return 0;
          rv = ::fgetxattr(fd,attr.c_str(),&value[0],size);
        }

      return rv;
#else
      errno = ENOTSUP;
      return -1;
#endif
    }

    int
    get(const string &path,
        const string &attr,
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
          if(size == 0)
            return 0;
          rv = ::getxattr(path.c_str(),attr.c_str(),&value[0],size);
        }

      return rv;
#else
      errno = ENOTSUP;
      return -1;
#endif
    }

    int
    get(const int     fd,
        const string &attr,
        string       &value)
    {
      int          rv;
      vector<char> tmpvalue;

      rv = get(fd,attr,tmpvalue);
      if(rv != -1)
        value = string(tmpvalue.begin(),tmpvalue.end());

      return rv;
    }

    int
    get(const string &path,
        const string &attr,
        string       &value)
    {
      int          rv;
      vector<char> tmpvalue;

      rv = get(path,attr,tmpvalue);
      if(rv != -1)
        value = string(tmpvalue.begin(),tmpvalue.end());

      return rv;
    }

    int
    get(const int           fd,
        map<string,string> &attrs)
    {
      int rv;
      string attrstr;

      rv = list(fd,attrstr);
      if(rv == -1)
        return -1;

      {
        string key;
        istringstream ss(attrstr);

        while(getline(ss,key,'\0'))
          {
            string value;

            rv = get(fd,key,value);
            if(rv != -1)
              attrs[key] = value;
          }
      }

      return 0;
    }

    int
    get(const string       &path,
        map<string,string> &attrs)
    {
      int rv;
      string attrstr;

      rv = list(path,attrstr);
      if(rv == -1)
        return -1;

      {
        string key;
        istringstream ss(attrstr);

        while(getline(ss,key,'\0'))
          {
            string value;

            rv = get(path,key,value);
            if(rv != -1)
              attrs[key] = value;
          }
      }

      return 0;
    }

    int
    set(const int     fd,
        const string &key,
        const string &value,
        const int     flags)
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
    set(const string &path,
        const string &key,
        const string &value,
        const int     flags)
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
    set(const int                 fd,
        const map<string,string> &attrs)
    {
      int rv;

      for(map<string,string>::const_iterator
            i = attrs.begin(), ei = attrs.end(); i != ei; ++i)
        {
          rv = set(fd,i->first,i->second,0);
          if(rv == -1)
            return -1;
        }

      return 0;
    }

    int
    set(const string             &path,
        const map<string,string> &attrs)
    {
      int fd;

      fd = ::open(path.c_str(),O_RDONLY|O_NONBLOCK);
      if(fd == -1)
        return -1;

      set(fd,attrs);

      return ::close(fd);
    }

    int
    copy(const int fdin,
         const int fdout)
    {
      int rv;
      map<string,string> attrs;

      rv = get(fdin,attrs);
      if(rv == -1)
        return -1;

      return set(fdout,attrs);
    }

    int
    copy(const string &from,
         const string &to)
    {
      int rv;
      map<string,string> attrs;

      rv = get(from,attrs);
      if(rv == -1)
        return -1;

      return set(to,attrs);
    }
  }
}

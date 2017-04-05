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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "errno.hpp"
#include "fs_base_open.hpp"
#include "fs_base_close.hpp"
#include "str.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using std::map;
using std::istringstream;

/*
  The Mac version of the get/set APIs includes a position arg to seek around the
  resource fork; for all other uses, the value is 0.

  For the other APIs, there are no link-specific variants; rather the standard call
  has a flags argument where XATTR_NOFOLLOW is specified to address the link itself.
*/

#if __APPLE__
  ssize_t
  _flistxattr(int fd, char* namebuf, size_t size)
  {
    return ::flistxattr(fd, namebuf, size, 0);
  }
  
  ssize_t
  _llistxattr(const char* path, char* namebuf, size_t size)
  {
    return ::listxattr(path, namebuf, size, XATTR_NOFOLLOW);
  }
  
  ssize_t
  _fgetxattr(int fd, const char* name, char* value, size_t size)
  {
    return ::fgetxattr(fd, name, value, size, 0, 0);
  }

  ssize_t
  _lgetxattr(const char* path, const char* name, char* value, size_t size)
  {
    return ::getxattr(path, name, value, size, 0, XATTR_NOFOLLOW);
  }
  
  ssize_t
  _fsetxattr(int fd, const char* name, const char* value, size_t size, int flags)
  {
    return ::fsetxattr(fd, name, value, size, 0, flags);
  }

  ssize_t
  _lsetxattr(const char* path, const char* name, const char* value, size_t size, int flags)
  {
    return ::setxattr(path, name, value, size, 0, flags && XATTR_NOFOLLOW);
  }
  
#else
  #define _flistxattr ::flistxattr
  #define _llistxattr ::llistxattr
  #define _fgetxattr  ::fgetxattr
  #define _lgetxattr  ::lgetxattr
  #define _fsetxattr  ::fsetxattr
  #define _lsetxattr  ::lsetxattr
#endif

namespace fs
{
  namespace xattr
  {
    ssize_t
    list(const int     fd,
         vector<char> &attrs)
    {
#ifndef WITHOUT_XATTR
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = _flistxattr(fd,NULL,0);
          if(rv <= 0)
            return rv;

          attrs.resize((size_t)rv);

          rv = _flistxattr(fd,&attrs[0],(size_t)rv);
        }

      return rv;
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    list(const string &path,
         vector<char> &attrs)
    {
#ifndef WITHOUT_XATTR
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = _llistxattr(path.c_str(),NULL,0);
          if(rv <= 0)
            return rv;

          attrs.resize((size_t)rv);

          rv = _llistxattr(path.c_str(),&attrs[0],(size_t)rv);
        }

      return rv;
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    list(const int        fd,
          vector<string> &attrvector)
    {
      ssize_t rv;
      vector<char> attrs;

      rv = list(fd,attrs);
      if(rv != -1)
        {
          string tmp(attrs.begin(),attrs.end());
          str::split(attrvector,tmp,'\0');
        }

      return rv;
    }

    ssize_t
    list(const string   &path,
         vector<string> &attrvector)
    {
      ssize_t rv;
      vector<char> attrs;

      rv = list(path,attrs);
      if(rv != -1)
        {
          string tmp(attrs.begin(),attrs.end());
          str::split(attrvector,tmp,'\0');
        }

      return rv;
    }

    ssize_t
    list(const int  fd,
         string    &attrstr)
    {
      ssize_t rv;
      vector<char> attrs;

      rv = list(fd,attrs);
      if(rv != -1)
        attrstr = string(attrs.begin(),attrs.end());

      return rv;
    }

    ssize_t
    list(const string &path,
         string       &attrstr)
    {
      ssize_t rv;
      vector<char> attrs;

      rv = list(path,attrs);
      if(rv != -1)
        attrstr = string(attrs.begin(),attrs.end());

      return rv;
    }

    ssize_t
    get(const int     fd,
        const string &attr,
        vector<char> &value)
    {
#ifndef WITHOUT_XATTR
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = _fgetxattr(fd,attr.c_str(),NULL,0);
          if(rv <= 0)
            return rv;

          value.resize((size_t)rv);

          rv = _fgetxattr(fd,attr.c_str(),&value[0],(size_t)rv);
        }

      return rv;
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    get(const string &path,
        const string &attr,
        vector<char> &value)
    {
#ifndef WITHOUT_XATTR
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = _lgetxattr(path.c_str(),attr.c_str(),NULL,0);
          if(rv <= 0)
            return rv;

          value.resize((size_t)rv);

          rv = _lgetxattr(path.c_str(),attr.c_str(),&value[0],(size_t)rv);
        }

      return rv;
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    get(const int     fd,
        const string &attr,
        string       &value)
    {
      ssize_t          rv;
      vector<char> tmpvalue;

      rv = get(fd,attr,tmpvalue);
      if(rv != -1)
        value = string(tmpvalue.begin(),tmpvalue.end());

      return rv;
    }

    ssize_t
    get(const string &path,
        const string &attr,
        string       &value)
    {
      ssize_t          rv;
      vector<char> tmpvalue;

      rv = get(path,attr,tmpvalue);
      if(rv != -1)
        value = string(tmpvalue.begin(),tmpvalue.end());

      return rv;
    }

    ssize_t
    get(const int           fd,
        map<string,string> &attrs)
    {
      ssize_t rv;
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

    ssize_t
    get(const string       &path,
        map<string,string> &attrs)
    {
      ssize_t rv;
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

    ssize_t
    set(const int     fd,
        const string &key,
        const string &value,
        const int     flags)
    {
#ifndef WITHOUT_XATTR
      return _fsetxattr(fd,
                        key.c_str(),
                        value.data(),
                        value.size(),
                        flags);
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    set(const string &path,
        const string &key,
        const string &value,
        const int     flags)
    {
#ifndef WITHOUT_XATTR
      return _lsetxattr(path.c_str(),
                        key.c_str(),
                        value.data(),
                        value.size(),
                        flags);
#else
      return (errno=ENOTSUP,-1);
#endif
    }

    ssize_t
    set(const int                 fd,
        const map<string,string> &attrs)
    {
      ssize_t rv;

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

      fd = fs::open(path,O_RDONLY|O_NONBLOCK);
      if(fd == -1)
        return -1;

      set(fd,attrs);

      return fs::close(fd);
    }

    ssize_t
    copy(const int fdin,
         const int fdout)
    {
      ssize_t rv;
      map<string,string> attrs;

      rv = get(fdin,attrs);
      if(rv == -1)
        return -1;

      return set(fdout,attrs);
    }

    ssize_t
    copy(const string &from,
         const string &to)
    {
      ssize_t rv;
      map<string,string> attrs;

      rv = get(from,attrs);
      if(rv == -1)
        return -1;

      return set(to,attrs);
    }
  }
}

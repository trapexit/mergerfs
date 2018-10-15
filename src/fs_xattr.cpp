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

#include "errno.hpp"
#include "fs_base_close.hpp"
#include "fs_base_getxattr.hpp"
#include "fs_base_listxattr.hpp"
#include "fs_base_open.hpp"
#include "fs_base_removexattr.hpp"
#include "fs_base_setxattr.hpp"
#include "str.hpp"

#include <string>
#include <vector>
#include <map>
#include <sstream>


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
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = fs::flistxattr(fd,NULL,0);
          if(rv <= 0)
            return rv;

          attrs.resize(rv);

          rv = fs::flistxattr(fd,&attrs[0],rv);
        }

      return rv;
    }

    int
    list(const string &path,
         vector<char> &attrs)
    {
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = fs::llistxattr(path,NULL,0);
          if(rv <= 0)
            return rv;

          attrs.resize(rv);

          rv = fs::llistxattr(path,&attrs[0],rv);
        }

      return rv;
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
         string    &attrstr)
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
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = fs::fgetxattr(fd,attr,NULL,0);
          if(rv <= 0)
            return rv;

          value.resize(rv);

          rv = fs::fgetxattr(fd,attr,&value[0],rv);
        }

      return rv;
    }

    int
    get(const string &path,
        const string &attr,
        vector<char> &value)
    {
      ssize_t rv;

      rv    = -1;
      errno = ERANGE;
      while((rv == -1) && (errno == ERANGE))
        {
          rv = fs::lgetxattr(path,attr,NULL,0);
          if(rv <= 0)
            return rv;

          value.resize(rv);

          rv = fs::lgetxattr(path,attr,&value[0],rv);
        }

      return rv;
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
      return fs::fsetxattr(fd,
                           key.c_str(),
                           value.data(),
                           value.size(),
                           flags);
    }

    int
    set(const string &path,
        const string &key,
        const string &value,
        const int     flags)
    {
      return fs::lsetxattr(path,
                           key,
                           value.data(),
                           value.size(),
                           flags);
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

      fd = fs::open(path,O_RDONLY|O_NONBLOCK);
      if(fd == -1)
        return -1;

      set(fd,attrs);

      return fs::close(fd);
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

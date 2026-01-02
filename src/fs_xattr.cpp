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

#include "fs_xattr.hpp"

#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_fgetxattr.hpp"
#include "fs_flistxattr.hpp"
#include "fs_fsetxattr.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_llistxattr.hpp"
#include "fs_lremovexattr.hpp"
#include "fs_lsetxattr.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "str.hpp"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#define TOOBIG_SIZE 65536


int
fs::xattr::list(const int     fd_,
                vector<char> *attrs_)
{
  ssize_t rv;

  attrs_->resize(4096);

  while(true)
    {
      rv = fs::flistxattr(fd_,attrs_->data(),attrs_->size());
      if(rv >= 0)
        {
          attrs_->resize(rv);
          return rv;
        }
      if(rv != -ERANGE)
        return rv;
      if(attrs_->size() > TOOBIG_SIZE)
        return -E2BIG;

      attrs_->resize(attrs_->size() * 1.2);
    }

  return rv;
}

int
fs::xattr::list(const string &path_,
                vector<char> *attrs_)
{
  ssize_t rv;

  attrs_->resize(4096);

  while(true)
    {
      rv = fs::llistxattr(path_,attrs_->data(),attrs_->size());
      if(rv >= 0)
        {
          attrs_->resize(rv);
          return rv;
        }
      if(rv != -ERANGE)
        return rv;
      if(attrs_->size() > TOOBIG_SIZE)
        return -E2BIG;

      attrs_->resize(attrs_->size() * 1.2);
    }

  return rv;
}

int
fs::xattr::list(const int       fd_,
                vector<string> *attrvector_)
{
  int rv;
  vector<char> attrs;

  rv = fs::xattr::list(fd_,&attrs);
  if(rv > 0)
    {
      string tmp(attrs.begin(),attrs.end());
      str::split(tmp,'\0',attrvector_);
    }

  return rv;
}

int
fs::xattr::list(const string   &path_,
                vector<string> *attrvector_)
{
  int rv;
  vector<char> attrs;

  rv = fs::xattr::list(path_,&attrs);
  if(rv > 0)
    {
      string tmp(attrs.begin(),attrs.end());
      str::split(tmp,'\0',attrvector_);
    }

  return rv;
}

int
fs::xattr::list(const int  fd_,
                string    *attrstr_)
{
  int rv;
  vector<char> attrs;

  rv = fs::xattr::list(fd_,&attrs);
  if(rv > 0)
    *attrstr_ = string{attrs.begin(),attrs.end()};

  return rv;
}

int
fs::xattr::list(const string &path_,
                string       *attrstr_)
{
  int rv;
  vector<char> attrs;

  rv = fs::xattr::list(path_,&attrs);
  if(rv > 0)
    *attrstr_ = string{attrs.begin(),attrs.end()};

  return rv;
}

int
fs::xattr::get(const int     fd_,
               const string &attr_,
               vector<char> *val_)
{
  ssize_t rv;

  rv = -ERANGE;
  val_->resize(64);

  while(true)
    {
      rv = fs::fgetxattr(fd_,attr_,val_->data(),val_->size());
      if(rv >= 0)
        {
          val_->resize(rv);
          return rv;
        }
      if(rv != -ERANGE)
        return rv;
      if(val_->size() > TOOBIG_SIZE)
        return -E2BIG;

      val_->resize(val_->size() * 1.2);
    }

  return 0;
}

int
fs::xattr::get(const string &path_,
               const string &attr_,
               vector<char> *val_)
{
  ssize_t rv;

  val_->resize(64);

  while(true)
    {
      rv = fs::lgetxattr(path_,attr_,val_->data(),val_->size());
      if(rv >= 0)
        {
          val_->resize(rv);
          return rv;
        }
      if(rv != -ERANGE)
        return rv;
      if(val_->size() > TOOBIG_SIZE)
        return -E2BIG;

      val_->resize(val_->size() * 1.2);
    }

  return rv;
}

int
fs::xattr::get(const int     fd_,
               const string &attr_,
               string       *val_)
{
  int          rv;
  vector<char> tmpvalue;

  rv = fs::xattr::get(fd_,attr_,&tmpvalue);
  if(rv > 0)
    *val_ = string{tmpvalue.begin(),tmpvalue.end()};

  return rv;
}

int
fs::xattr::get(const string &path_,
               const string &attr_,
               string       *val_)
{
  int rv;
  vector<char> tmpvalue;

  rv = fs::xattr::get(path_,attr_,&tmpvalue);
  if(rv > 0)
    *val_ = string{tmpvalue.begin(),tmpvalue.end()};

  return rv;
}

int
fs::xattr::get(const int           fd_,
               map<string,string> *attrs_)
{
  int rv;
  string attrstr;

  rv = fs::xattr::list(fd_,&attrstr);
  if(rv < 0)
    return rv;

  {
    string key;
    std::istringstream ss(attrstr);

    while(getline(ss,key,'\0'))
      {
        string value;

        rv = fs::xattr::get(fd_,key,&value);
        if(rv >= 0)
          (*attrs_)[key] = value;
      }
  }

  return 0;
}

int
fs::xattr::get(const string       &path_,
               map<string,string> *attrs_)
{
  int rv;
  string attrstr;

  rv = fs::xattr::list(path_,&attrstr);
  if(rv < 0)
    return rv;

  {
    string key;
    std::istringstream ss(attrstr);

    while(getline(ss,key,'\0'))
      {
        string value;

        rv = fs::xattr::get(path_,key,&value);
        if(rv >= 0)
          (*attrs_)[key] = value;
      }
  }

  return 0;
}

int
fs::xattr::set(const int     fd_,
               const string &key_,
               const string &val_,
               const int     flags_)
{
  return fs::fsetxattr(fd_,
                       key_,
                       val_.data(),
                       val_.size(),
                       flags_);
}

int
fs::xattr::set(const fs::path &path_,
               const string   &key_,
               const string   &val_,
               const int       flags_)
{
  return fs::lsetxattr(path_,
                       key_,
                       val_.data(),
                       val_.size(),
                       flags_);
}

int
fs::xattr::set(const int                 fd_,
               const map<string,string> &attrs_)
{
  int rv;

  for(const auto &[key,val] : attrs_)
    {
      rv = fs::xattr::set(fd_,key,val,0);
      if(rv < 0)
        return rv;
    }

  return 0;
}

int
fs::xattr::set(const string             &path_,
               const map<string,string> &attrs_)
{
  int fd;
  int rv;

  fd = fs::open(path_,O_RDONLY|O_NONBLOCK);
  if(fd < 0)
    return fd;

  rv = fs::xattr::set(fd,attrs_);

  fs::close(fd);

  return rv;
}

int
fs::xattr::copy(const int fdin_,
                const int fdout_)
{
  int rv;
  map<string,string> attrs;

  rv = fs::xattr::get(fdin_,&attrs);
  if(rv < 0)
    return rv;

  return fs::xattr::set(fdout_,attrs);
}

int
fs::xattr::copy(const string &from_,
                const string &to_)
{
  int rv;
  map<string,string> attrs;

  rv = fs::xattr::get(from_,&attrs);
  if(rv < 0)
    return rv;

  return fs::xattr::set(to_,attrs);
}

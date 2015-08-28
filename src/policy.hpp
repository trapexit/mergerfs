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

#ifndef __POLICY_HPP__
#define __POLICY_HPP__

#include <string>
#include <vector>
#include <map>

#include "path.hpp"
#include "fs.hpp"
#include "category.hpp"

namespace mergerfs
{
  class Policy
  {
  public:
    struct Enum
    {
      enum Type
        {
          invalid = -1,
          BEGIN   = 0,
          all     = BEGIN,
          einval,
          enosys,
          enotsup,
          epmfs,
          erofs,
          exdev,
          ff,
          ffwp,
          fwfs,
          lfs,
          mfs,
          newest,
          rand,
          END
        };
    };

    struct Func
    {
      typedef std::string string;
      typedef std::size_t size_t;
      typedef std::vector<string> strvec;
      typedef const string cstring;
      typedef const size_t csize_t;
      typedef const strvec cstrvec;
      typedef const Category::Enum::Type CType;

      typedef int (*Ptr)(CType,cstrvec&,cstring&,csize_t,strvec&);

      class Action
      {
      public:
        Action(const Policy *p)
          : func(p->_func)
        {}

        int
        operator()(cstrvec& b,cstring& c,csize_t d,strvec& e)
        {
          return func(Category::Enum::action,b,c,d,e);
        }

      private:
        const Ptr func;
      };

      class Create
      {
      public:
        Create(const Policy *p)
          : func(p->_func)
        {}

        int
        operator()(cstrvec& b,cstring& c,csize_t d,strvec& e)
        {
          return func(Category::Enum::create,b,c,d,e);
        }

      private:
        const Ptr func;
      };

      class Search
      {
      public:
        Search(const Policy *p)
          : func(p->_func)
        {}

        int
        operator()(cstrvec& b,cstring& c,csize_t d,strvec& e)
        {
          return func(Category::Enum::search,b,c,d,e);
        }

      private:
        const Ptr func;
      };

      static int all(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int einval(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int enosys(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int enotsup(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int epmfs(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int erofs(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int exdev(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int ff(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int ffwp(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int fwfs(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int invalid(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int lfs(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int mfs(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int newest(CType,cstrvec&,cstring&,csize_t,strvec&);
      static int rand(CType,cstrvec&,cstring&,csize_t,strvec&);
    };

  private:
    Enum::Type  _enum;
    std::string _str;
    Func::Ptr   _func;

  public:
    Policy()
      : _enum(invalid),
        _str(invalid),
        _func(invalid)
    {
    }

    Policy(const Enum::Type   enum_,
           const std::string &str_,
           const Func::Ptr    func_)
      : _enum(enum_),
        _str(str_),
        _func(func_)
    {
    }

  public:
    operator const Enum::Type() const { return _enum; }
    operator const std::string&() const { return _str; }
    operator const Func::Ptr() const { return _func; }
    operator const Policy*() const { return this; }

    bool operator==(const Enum::Type enum_) const
    { return _enum == enum_; }
    bool operator==(const std::string &str_) const
    { return _str == str_; }
    bool operator==(const Func::Ptr func_) const
    { return _func == func_; }

    bool operator!=(const Policy &r) const
    { return _enum != r._enum; }

    bool operator<(const Policy &r) const
    { return _enum < r._enum; }

  public:
    static const Policy &find(const std::string&);
    static const Policy &find(const Enum::Type);

  public:
    static const std::vector<Policy>  _policies_;
    static const Policy * const       policies;

    static const Policy &invalid;
    static const Policy &all;
    static const Policy &enosys;
    static const Policy &enotsup;
    static const Policy &epmfs;
    static const Policy &erofs;
    static const Policy &exdev;
    static const Policy &ff;
    static const Policy &ffwp;
    static const Policy &fwfs;
    static const Policy &lfs;
    static const Policy &mfs;
    static const Policy &newest;
    static const Policy &rand;
  };
}

#endif

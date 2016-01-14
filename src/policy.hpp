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

#ifndef __POLICY_HPP__
#define __POLICY_HPP__

#include <string>
#include <vector>
#include <map>

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

      static size_t begin() { return BEGIN; }
      static size_t end()   { return END; }
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

      template <CType T>
      class Base
      {
      public:
        Base(const Policy *p)
          : func(p->_func)
        {}

        int
        operator()(cstrvec& b,cstring& c,csize_t d,strvec& e)
        {
          return func(T,b,c,d,e);
        }

        int
        operator()(cstrvec& b,cstring& c,csize_t d,string& e)
        {
          int rv;
          strvec vec;

          rv = func(T,b,c,d,vec);
          if(!vec.empty())
            e = vec[0];

          return rv;
        }

      private:
        const Ptr func;
      };

      typedef Base<Category::Enum::action> Action;
      typedef Base<Category::Enum::create> Create;
      typedef Base<Category::Enum::search> Search;

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
    static const Policy &einval;
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

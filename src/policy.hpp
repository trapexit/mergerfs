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

#pragma once

#include "branch.hpp"
#include "category.hpp"

#include <string>
#include <vector>

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
        epall,
        epff,
        eplfs,
        eplus,
        epmfs,
        eppfrd,
        eprand,
        erofs,
        ff,
        lfs,
        lus,
        mfs,
        msplfs,
        msplus,
        mspmfs,
        msppfrd,
        newest,
        pfrd,
        rand,
        END
      };

    static size_t begin() { return BEGIN; }
    static size_t end()   { return END; }
  };

  struct Func
  {
    typedef std::string string;
    typedef std::vector<string> strvec;
    typedef const string cstring;
    typedef const uint64_t cuint64_t;
    typedef const strvec cstrvec;

    typedef int (*Ptr)(Category,const Branches&,const char*,strvec *);

    template <Category T>
    class Base
    {
    public:
      Base(const Policy &p_)
        : func(p_._func)
      {}

      Base(const Policy *p_)
        : func(p_->_func)
      {}

      int
      operator()(const Branches &a,const char *b,strvec *c)
      {
        return func(T,a,b,c);
      }

      int
      operator()(const Branches &a,const string &b,strvec *c)
      {
        return func(T,a,b.c_str(),c);
      }

      int
      operator()(const Branches &a,const char *b,string *c)
      {
        int rv;
        strvec v;

        rv = func(T,a,b,&v);
        if(!v.empty())
          *c = v[0];

        return rv;
      }

    private:
      const Ptr func;
    };

    typedef Base<Category::ACTION> Action;
    typedef Base<Category::CREATE> Create;
    typedef Base<Category::SEARCH> Search;

    static int invalid(Category,const Branches&,const char *,strvec*);
    static int all(Category,const Branches&,const char*,strvec*);
    static int epall(Category,const Branches&,const char*,strvec*);
    static int epff(Category,const Branches&,const char *,strvec*);
    static int eplfs(Category,const Branches&,const char *,strvec*);
    static int eplus(Category,const Branches&,const char *,strvec*);
    static int epmfs(Category,const Branches&,const char *,strvec*);
    static int eppfrd(Category,const Branches&,const char *,strvec*);
    static int eprand(Category,const Branches&,const char *,strvec*);
    static int erofs(Category,const Branches&,const char *,strvec*);
    static int ff(Category,const Branches&,const char *,strvec*);
    static int lfs(Category,const Branches&,const char *,strvec*);
    static int lus(Category,const Branches&,const char *,strvec*);
    static int mfs(Category,const Branches&,const char *,strvec*);
    static int msplfs(Category,const Branches&,const char *,strvec*);
    static int msplus(Category,const Branches&,const char *,strvec*);
    static int mspmfs(Category,const Branches&,const char *,strvec*);
    static int msppfrd(Category,const Branches&,const char *,strvec*);
    static int newest(Category,const Branches&,const char *,strvec*);
    static int pfrd(Category,const Branches&,const char *,strvec*);
    static int rand(Category,const Branches&,const char *,strvec*);
  };

private:
  Enum::Type  _enum;
  std::string _str;
  Func::Ptr   _func;
  bool        _path_preserving;

public:
  Policy()
    : _enum(invalid),
      _str(invalid),
      _func(invalid),
      _path_preserving(false)
  {
  }

  Policy(const Enum::Type   enum_,
         const std::string &str_,
         const Func::Ptr    func_,
         const bool         path_preserving_)
    : _enum(enum_),
      _str(str_),
      _func(func_),
      _path_preserving(path_preserving_)
  {
  }

  bool
  path_preserving() const
  {
    return _path_preserving;
  }

public:
  operator const Enum::Type() const { return _enum; }
  operator const std::string&() const { return _str; }
  operator const Func::Ptr() const { return _func; }
  operator const Policy*() const { return this; }
  const std::string& to_string() const { return _str; }

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

  static const std::vector<Policy> _policies_;
  static const Policy * const      policies;

  static const Policy &invalid;
  static const Policy &all;
  static const Policy &epall;
  static const Policy &epff;
  static const Policy &eplfs;
  static const Policy &eplus;
  static const Policy &epmfs;
  static const Policy &eppfrd;
  static const Policy &eprand;
  static const Policy &erofs;
  static const Policy &ff;
  static const Policy &lfs;
  static const Policy &lus;
  static const Policy &mfs;
  static const Policy &msplfs;
  static const Policy &msplus;
  static const Policy &mspmfs;
  static const Policy &msppfrd;
  static const Policy &newest;
  static const Policy &pfrd;
  static const Policy &rand;
};

namespace std
{
  static
  inline
  string
  to_string(const Policy &p_)
  {
    return p_.to_string();
  }
}

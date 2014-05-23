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

#include "fs.hpp"

namespace mergerfs
{
  class Policy
  {
  public:
    enum Type
      {
        ACTION,
        CREATE,
        SEARCH
      };

  public:
    class Create
    {
    public:
      typedef fs::SearchFunc Func;
      enum Type
        {
          Invalid = -1,
          ExistingPath,
          MostFreeSpace,
          ExistingPathMostFreeSpace,
          Random,
          Max
        };

    public:
      Create(Type);

      operator Type() const { return _value; }
      operator Func() const { return _func;  }
      operator std::string() const { return _str; }
      std::string str() const { return _str; }

      Create& operator=(const Type);
      Create& operator=(const std::string);

      static Type        fromString(const std::string);
      static std::string toString(const Type);
      static Func        findFunc(const Type);

    private:
      Create();

    private:
      Type        _value;
      std::string _str;
      Func        _func;
    };

    class Search
    {
    public:
      typedef fs::SearchFunc Func;
      enum Type
        {
          Invalid = -1,
          FirstFound,
          FirstFoundWithPermissions,
          Newest,
          Max
        };

    public:
      Search(Type);

      operator Type() const { return _value; }
      operator Func() const { return _func;  }
      operator std::string() const { return _str; }
      std::string str() const { return _str; }

      Search& operator=(const Type);
      Search& operator=(const std::string);

      static Type        fromString(const std::string);
      static std::string toString(const Type);
      static Func        findFunc(const Type);

    private:
      Search();

    private:
      Type        _value;
      std::string _str;
      Func        _func;
    };

    class Action
    {
    public:
      typedef fs::VecSearchFunc Func;
      enum Type
        {
          Invalid = -1,
          FirstFound,
          FirstFoundWithPermissions,
          Newest,
          All,
          Max
        };

    public:
      Action(Type);

      operator Type() const { return _value; }
      operator Func() const { return _func;  }
      operator std::string() const { return _str; }
      std::string str() const { return _str; }

      Action& operator=(const Type);
      Action& operator=(const std::string);

      static Type        fromString(const std::string);
      static std::string toString(const Type);
      static Func        findFunc(const Type);

    private:
      Action();

    private:
      Type        _value;
      std::string _str;
      Func        _func;
    };

  public:
    Policy() :
      create(Create::ExistingPathMostFreeSpace),
      search(Search::FirstFound),
      action(Action::FirstFound)
    {}

  public:
    Create create;
    Search search;
    Action action;
  };
}

#endif

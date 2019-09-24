/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "policy.hpp"
#include "tofrom_string.hpp"

#include <string>
#include <iostream>

class Func : public ToFromString
{
public:
  Func(const Policy &policy_)
    : policy(&policy_)
  {
  }

public:
  int from_string(const std::string &s);
  std::string to_string() const;

public:
  const Policy *policy;
};

class FuncAccess : public Func
{
public:
  FuncAccess()
    : Func(Policy::ff)
  {
  }
};

class FuncChmod : public Func
{
public:
  FuncChmod()
    : Func(Policy::epall)
  {
  }
};

class FuncChown : public Func
{
public:
  FuncChown()
    : Func(Policy::epall)
  {
  }
};

class FuncCreate : public Func
{
public:
  FuncCreate()
    : Func(Policy::epmfs)
  {
  }
};

class FuncGetAttr : public Func
{
public:
  FuncGetAttr()
    : Func(Policy::ff)
  {
  }
};

class FuncGetXAttr : public Func
{
public:
  FuncGetXAttr()
    : Func(Policy::ff)
  {
  }
};

class FuncLink : public Func
{
public:
  FuncLink()
    : Func(Policy::epall)
  {
  }
};

class FuncListXAttr : public Func
{
public:
  FuncListXAttr()
    : Func(Policy::ff)
  {
  }
};

class FuncMkdir : public Func
{
public:
  FuncMkdir()
    : Func(Policy::epmfs)
  {
  }
};

class FuncMknod : public Func
{
public:
  FuncMknod()
    : Func(Policy::epmfs)
  {
  }
};

class FuncOpen : public Func
{
public:
  FuncOpen()
    : Func(Policy::ff)
  {
  }
};

class FuncReadlink : public Func
{
public:
  FuncReadlink()
    : Func(Policy::ff)
  {
  }
};

class FuncRemoveXAttr : public Func
{
public:
  FuncRemoveXAttr()
    : Func(Policy::epall)
  {
  }
};

class FuncRename : public Func
{
public:
  FuncRename()
    : Func(Policy::epall)
  {
  }
};

class FuncRmdir : public Func
{
public:
  FuncRmdir()
    : Func(Policy::epall)
  {
  }
};

class FuncSetXAttr : public Func
{
public:
  FuncSetXAttr()
    : Func(Policy::epall)
  {
  }
};

class FuncSymlink : public Func
{
public:
  FuncSymlink()
    : Func(Policy::epmfs)
  {
  }
};

class FuncTruncate : public Func
{
public:
  FuncTruncate()
    : Func(Policy::epall)
  {
  }
};

class FuncUnlink : public Func
{
public:
  FuncUnlink()
    : Func(Policy::epall)
  {
  }
};

class FuncUtimens : public Func
{
public:
  FuncUtimens()
    : Func(Policy::epall)
  {
  }
};

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

#include "policy.hpp"
#include "fs.hpp"

namespace mergerfs
{
  Policy::Create::Create(Type value)
  {
    *this = value;
  }

  Policy::Create&
  Policy::Create::operator=(const Policy::Create::Type value)
  {
    _str   = toString(value);
    _value = fromString(_str);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Create&
  Policy::Create::operator=(const std::string str)
  {
    _value = fromString(str);
    _str   = toString(_value);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Create::Type
  Policy::Create::fromString(const std::string str)
  {
    if(str == "ep")
      return Policy::Create::ExistingPath;
    else if(str == "mfs")
      return Policy::Create::MostFreeSpace;
    else if(str == "epmfs")
      return Policy::Create::ExistingPathMostFreeSpace;
    else if(str == "rand")
      return Policy::Create::Random;

    return Policy::Create::Invalid;
  }

  std::string
  Policy::Create::toString(const Type value)
  {
    switch(value)
      {
      case Policy::Create::ExistingPath:
        return "ep";
      case Policy::Create::ExistingPathMostFreeSpace:
        return "epmfs";
      case Policy::Create::MostFreeSpace:
        return "mfs";
      case Policy::Create::Random:
        return "rand";
      case Policy::Create::Invalid:
      default:
        return "invalid";
      }
  }

  Policy::Create::Func
  Policy::Create::findFunc(Policy::Create::Type value)
  {
    switch(value)
      {
      case Policy::Create::ExistingPath:
        return fs::find_first;
      case Policy::Create::ExistingPathMostFreeSpace:
        return fs::find_mfs_existing;
      case Policy::Create::MostFreeSpace:
        return fs::find_mfs;
      case Policy::Create::Random:
        return fs::find_random;
      case Policy::Create::Invalid:
      default:
        return NULL;
      }
  }

  Policy::Search::Search(Type value)
  {
    *this = value;
  }

  Policy::Search&
  Policy::Search::operator=(const Policy::Search::Type value)
  {
    _str   = toString(value);
    _value = fromString(_str);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Search&
  Policy::Search::operator=(const std::string str)
  {
    _value = fromString(str);
    _str   = toString(_value);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Search::Type
  Policy::Search::fromString(const std::string str)
  {
    if(str == "ff")
      return Policy::Search::FirstFound;
    else if(str == "ffwp")
      return Policy::Search::FirstFoundWithPermissions;
    else if(str == "newest")
      return Policy::Search::Newest;

    return Policy::Search::Invalid;
  }

  std::string
  Policy::Search::toString(Policy::Search::Type value)
  {
    switch(value)
      {
      case Policy::Search::FirstFound:
        return "ff";
      case Policy::Search::FirstFoundWithPermissions:
        return "ffwp";
      case Policy::Search::Newest:
        return "newest";
      case Policy::Search::Invalid:
      default:
        return "invalid";
      }
  }

  Policy::Search::Func
  Policy::Search::findFunc(Policy::Search::Type value)
  {
    switch(value)
      {
      case Policy::Search::FirstFound:
        return fs::find_first;
      case Policy::Search::FirstFoundWithPermissions:
        return fs::find_first_with_permission;
      case Policy::Search::Newest:
        return fs::find_newest;
      case Policy::Search::Invalid:
      default:
        return NULL;
      }
  }

  Policy::Action::Action(const Policy::Action::Type value)
  {
    *this = value;
  }

  Policy::Action&
  Policy::Action::operator=(const Policy::Action::Type value)
  {
    _str   = toString(value);
    _value = fromString(_str);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Action&
  Policy::Action::operator=(const std::string str)
  {
    _value = fromString(str);
    _str   = toString(_value);
    _func  = findFunc(_value);

    return *this;
  }

  Policy::Action::Type
  Policy::Action::fromString(const std::string str)
  {
    if(str == "ff")
      return Policy::Action::FirstFound;
    else if(str == "ffwp")
      return Policy::Action::FirstFoundWithPermissions;
    else if(str == "newest")
      return Policy::Action::Newest;
    else if(str == "all")
      return Policy::Action::All;

    return Policy::Action::Invalid;
  }

  std::string
  Policy::Action::toString(Policy::Action::Type value)
  {
    switch(value)
      {
      case Policy::Action::FirstFound:
        return "ff";
      case Policy::Action::FirstFoundWithPermissions:
        return "ffwp";
      case Policy::Action::Newest:
        return "newest";
      case Policy::Action::All:
        return "all";
      case Policy::Action::Invalid:
      default:
        return "invalid";
      }
  }

  Policy::Action::Func
  Policy::Action::findFunc(const Policy::Action::Type value)
  {
    switch(value)
      {
      case Policy::Action::FirstFound:
        return fs::find_first;
      case Policy::Action::FirstFoundWithPermissions:
        return fs::find_first_with_permission;
      case Policy::Action::Newest:
        return fs::find_newest;
      case Policy::Action::All:
        return fs::find_all;
      case Policy::Action::Invalid:
      default:
        return NULL;
      }
  }
}

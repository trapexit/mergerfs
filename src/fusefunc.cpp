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

#include <string>
#include <vector>

#include "fusefunc.hpp"
#include "category.hpp"
#include "buildvector.hpp"

#define FUSEFUNC(X,Y) FuseFunc(FuseFunc::Enum::X,#X,Category::Enum::Y)

namespace mergerfs
{
  const std::vector<FuseFunc> FuseFunc::_fusefuncs_ =
    buildvector<FuseFunc,true>
    (FUSEFUNC(invalid,invalid))
    (FUSEFUNC(access,search))
    (FUSEFUNC(chmod,action))
    (FUSEFUNC(chown,action))
    (FUSEFUNC(create,create))
    (FUSEFUNC(getattr,search))
    (FUSEFUNC(getxattr,search))
    (FUSEFUNC(link,action))
    (FUSEFUNC(listxattr,search))
    (FUSEFUNC(mkdir,create))
    (FUSEFUNC(mknod,create))
    (FUSEFUNC(open,search))
    (FUSEFUNC(readlink,search))
    (FUSEFUNC(removexattr,action))
    (FUSEFUNC(rename,action))
    (FUSEFUNC(rmdir,action))
    (FUSEFUNC(setxattr,action))
    (FUSEFUNC(symlink,create))
    (FUSEFUNC(truncate,action))
    (FUSEFUNC(unlink,action))
    (FUSEFUNC(utimens,action))
    ;

  const FuseFunc * const FuseFunc::fusefuncs = &_fusefuncs_[1];

  const FuseFunc &FuseFunc::invalid     = FuseFunc::fusefuncs[FuseFunc::Enum::invalid];
  const FuseFunc &FuseFunc::access      = FuseFunc::fusefuncs[FuseFunc::Enum::access];
  const FuseFunc &FuseFunc::chmod       = FuseFunc::fusefuncs[FuseFunc::Enum::chmod];
  const FuseFunc &FuseFunc::chown       = FuseFunc::fusefuncs[FuseFunc::Enum::chown];
  const FuseFunc &FuseFunc::create      = FuseFunc::fusefuncs[FuseFunc::Enum::create];
  const FuseFunc &FuseFunc::getattr     = FuseFunc::fusefuncs[FuseFunc::Enum::getattr];
  const FuseFunc &FuseFunc::getxattr    = FuseFunc::fusefuncs[FuseFunc::Enum::getxattr];
  const FuseFunc &FuseFunc::link        = FuseFunc::fusefuncs[FuseFunc::Enum::link];
  const FuseFunc &FuseFunc::listxattr   = FuseFunc::fusefuncs[FuseFunc::Enum::listxattr];
  const FuseFunc &FuseFunc::mkdir       = FuseFunc::fusefuncs[FuseFunc::Enum::mkdir];
  const FuseFunc &FuseFunc::mknod       = FuseFunc::fusefuncs[FuseFunc::Enum::mknod];
  const FuseFunc &FuseFunc::open        = FuseFunc::fusefuncs[FuseFunc::Enum::open];
  const FuseFunc &FuseFunc::readlink    = FuseFunc::fusefuncs[FuseFunc::Enum::readlink];
  const FuseFunc &FuseFunc::removexattr = FuseFunc::fusefuncs[FuseFunc::Enum::removexattr];
  const FuseFunc &FuseFunc::rmdir       = FuseFunc::fusefuncs[FuseFunc::Enum::rmdir];
  const FuseFunc &FuseFunc::setxattr    = FuseFunc::fusefuncs[FuseFunc::Enum::setxattr];
  const FuseFunc &FuseFunc::symlink     = FuseFunc::fusefuncs[FuseFunc::Enum::symlink];
  const FuseFunc &FuseFunc::truncate    = FuseFunc::fusefuncs[FuseFunc::Enum::truncate];
  const FuseFunc &FuseFunc::unlink      = FuseFunc::fusefuncs[FuseFunc::Enum::unlink];
  const FuseFunc &FuseFunc::utimens     = FuseFunc::fusefuncs[FuseFunc::Enum::utimens];

  const FuseFunc&
  FuseFunc::find(const std::string &str)
  {
    for(int i = Enum::BEGIN; i != Enum::END; ++i)
      {
        if(fusefuncs[i] == str)
          return fusefuncs[i];
      }

    return invalid;
  }

  const FuseFunc&
  FuseFunc::find(const FuseFunc::Enum::Type i)
  {
    if(i >= FuseFunc::Enum::BEGIN &&
       i  < FuseFunc::Enum::END)
      return fusefuncs[i];

    return invalid;
  }
}

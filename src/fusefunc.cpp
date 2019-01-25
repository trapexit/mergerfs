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

#include <string>
#include <vector>

#include "buildvector.hpp"
#include "category.hpp"
#include "fusefunc.hpp"

#define FUSEFUNC(X,Y) FuseFunc(FuseFunc::Enum::X,#X,Category::Enum::Y)

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

const
FuseFunc&
FuseFunc::find(const std::string &str)
{
  for(int i = Enum::BEGIN; i != Enum::END; ++i)
    {
      if(fusefuncs[i] == str)
        return fusefuncs[i];
    }

  return invalid;
}

const
FuseFunc&
FuseFunc::find(const FuseFunc::Enum::Type i)
{
  if((i >= FuseFunc::Enum::BEGIN) && (i  < FuseFunc::Enum::END))
    return fusefuncs[i];

  return invalid;
}

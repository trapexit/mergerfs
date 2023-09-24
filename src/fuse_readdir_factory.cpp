/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_readdir_factory.hpp"

#include "fuse_readdir_cor.hpp"
#include "fuse_readdir_cosr.hpp"
#include "fuse_readdir_seq.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

#define DEFAULT_MAX_QUEUE_DEPTH 3

namespace l
{
  static
  void
  read_cfg(std::string const  str_,
           std::string       &type_,
           unsigned          &concurrency_,
           unsigned          &max_queue_depth_)
  {
    char type[16];
    int concurrency;
    int max_queue_depth;

    concurrency = 0;
    max_queue_depth = 0;
    std::sscanf(str_.c_str(),
                "%15[a-z]:%d:%d",
                type,
                &concurrency,
                &max_queue_depth);

    if(concurrency == 0)
      concurrency = std::thread::hardware_concurrency();
    else if(concurrency < 0)
      concurrency = (std::thread::hardware_concurrency() / std::abs(concurrency));

    if(concurrency == 0)
      concurrency = 1;

    if(max_queue_depth == 0)
      max_queue_depth = DEFAULT_MAX_QUEUE_DEPTH;

    max_queue_depth *= concurrency;

    type_            = type;
    concurrency_     = concurrency;
    max_queue_depth_ = max_queue_depth;
  }
}

bool
FUSE::ReadDirFactory::valid(std::string const str_)
{
  unsigned concurrency;
  unsigned max_queue_depth;
  std::string type;
  static const std::set<std::string> types =
    {
      "seq", "cosr", "cor"
    };

  l::read_cfg(str_,type,concurrency,max_queue_depth);

  if(types.find(type) == types.end())
    return false;
  if(concurrency <= 0)
    return false;

  return true;
}

std::shared_ptr<FUSE::ReadDirBase>
FUSE::ReadDirFactory::make(std::string const str_)
{
  unsigned concurrency;
  unsigned max_queue_depth;
  std::string type;

  if(!valid(str_))
    return {};

  l::read_cfg(str_,type,concurrency,max_queue_depth);

  if(type == "seq")
    return std::make_shared<FUSE::ReadDirSeq>();
  if(type == "cosr")
    return std::make_shared<FUSE::ReadDirCOSR>(concurrency,max_queue_depth);
  if(type == "cor")
    return std::make_shared<FUSE::ReadDirCOR>(concurrency,max_queue_depth);

  return {};
}

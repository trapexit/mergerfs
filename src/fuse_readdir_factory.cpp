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


namespace l
{
  static
  void
  read_cfg(std::string const  cfgstr_,
           std::string       &type_,
           int               &concurrency_)
  {
    char type[16];
    int concurrency;

    concurrency = 0;
    std::sscanf(cfgstr_.c_str(),"%15[a-z]:%u",type,&concurrency);

    if(concurrency == 0)
      concurrency = std::thread::hardware_concurrency();
    else if(concurrency < 0)
      concurrency = (std::thread::hardware_concurrency() / std::abs(concurrency));

    if(concurrency == 0)
      concurrency = 1;

    type_        = type;
    concurrency_ = concurrency;
  }
}

std::shared_ptr<FUSE::ReadDirBase>
FUSE::ReadDirFactory::make(std::string const cfgstr_)
{
  int concurrency;
  std::string type;

  l::read_cfg(cfgstr_,type,concurrency);
  assert(concurrency);

  if(type == "seq")
    return std::make_shared<FUSE::ReadDirSeq>();
  if(type == "cosr")
    return std::make_shared<FUSE::ReadDirCOSR>(concurrency);
  if(type == "cor")
    return std::make_shared<FUSE::ReadDirCOR>(concurrency);

  return {};
}

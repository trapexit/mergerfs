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

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <regex>

// Regex:
// ^([a-z]{1,15}) -> required type
// (?::(\d+))?    -> optional colon + concurrency
// (?::(\d+))?    -> optional colon + max_queue_depth
// $              -> end of string
static
void
_read_cfg(const std::string_view  str_,
          std::string            &type_,
          int                    &concurrency_,
          int                    &max_queue_depth_)
{
  std::string type;
  int concurrency;
  int max_queue_depth;
  bool matched;
  std::cmatch match;
  std::regex re(R"(^([a-z]{1,15})(?::(\d+))?(?::(\d+))?$)");

  concurrency     = 0;
  max_queue_depth = 0;
  matched = std::regex_match(str_.begin(),
                             str_.end(),
                             match,
                             re);
  if(matched)
    {
      type = match[1];
      if(match[2].matched)
        concurrency = std::stoi(match[2]);
      if(match[3].matched)
        max_queue_depth = std::stoi(match[3]);
    }

  if(concurrency == 0)
    {
      concurrency = std::thread::hardware_concurrency();
      if(concurrency > 8)
        concurrency = 8;
    }
  else if(concurrency < 0)
    {
      concurrency = (std::thread::hardware_concurrency() / std::abs(concurrency));
    }

  if(concurrency <= 0)
    concurrency = 1;

  if(max_queue_depth <= 0)
    max_queue_depth = 2;

  max_queue_depth *= concurrency;

  type_            = type;
  concurrency_     = concurrency;
  max_queue_depth_ = max_queue_depth;
}

bool
FUSE::ReadDirFactory::valid(const std::string_view str_)
{
  int concurrency;
  int max_queue_depth;
  std::string type;

  ::_read_cfg(str_,type,concurrency,max_queue_depth);

  return ((type == "seq") ||
          (type == "cosr") ||
          (type == "cor"));
}

std::shared_ptr<FUSE::ReadDirBase>
FUSE::ReadDirFactory::make(const std::string_view str_)
{
  int concurrency;
  int max_queue_depth;
  std::string type;

  ::_read_cfg(str_,type,concurrency,max_queue_depth);

  if(type == "seq")
    return std::make_shared<FUSE::ReadDirSeq>();
  if(type == "cosr")
    return std::make_shared<FUSE::ReadDirCOSR>(concurrency,max_queue_depth);
  if(type == "cor")
    return std::make_shared<FUSE::ReadDirCOR>(concurrency,max_queue_depth);

  return {};
}

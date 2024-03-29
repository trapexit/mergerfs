/*
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

#include "fuse.h"
#include "tofrom_string.hpp"
#include "fuse_readdir_base.hpp"

#include <memory>
#include <mutex>

#include <assert.h>

// The initialization behavior is not pretty but for the moment
// needed due to the daemonizing function of the libfuse library when
// not using foreground mode. The threads need to be created after the
// fork, not before.
namespace FUSE
{
  int readdir(fuse_file_info_t const *ffi,
              fuse_dirents_t         *buf);

  class ReadDir : public ToFromString
  {
  public:
    ReadDir(std::string const s_);

  public:
    std::string to_string() const;
    int from_string(const std::string &);

  public:
    int operator()(fuse_file_info_t const *ffi,
                   fuse_dirents_t         *buf);

  public:
    void initialize();

  private:
    mutable std::mutex _mutex;

  private:
    bool _initialized;
    std::string _type;
    std::shared_ptr<FUSE::ReadDirBase> _readdir;
  };
}

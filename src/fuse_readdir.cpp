/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_readdir.hpp"

#include "config.hpp"
#include "fuse_readdir_factory.hpp"

/*
  The _initialized stuff is not pretty but easiest way to deal with
  the fact that mergerfs is doing arg parsing and setting up of things
  (including thread pools) before the daemonizing
 */

int
FUSE::readdir(const fuse_file_info_t *ffi_,
              fuse_dirents_t         *buf_)
{
  Config::Write cfg;

  return cfg->readdir(ffi_,buf_);
}

FUSE::ReadDir::ReadDir(std::string const s_)
  : _initialized(false)
{
  from_string(s_);
  if(_initialized)
    assert(_readdir);
}

std::string
FUSE::ReadDir::to_string() const
{
  std::lock_guard<std::mutex> lg(_mutex);

  return _type;
}

void
FUSE::ReadDir::initialize()
{
  _initialized = true;
  from_string(_type);
}

int
FUSE::ReadDir::from_string(std::string const &str_)
{
  if(_initialized)
    {
      std::shared_ptr<FUSE::ReadDirBase> tmp;

      tmp = FUSE::ReadDirFactory::make(str_);
      if(!tmp)
        return -EINVAL;

      {
        std::lock_guard<std::mutex> lg(_mutex);

        _type    = str_;
        std::swap(_readdir,tmp);
      }
    }
  else
    {
      std::lock_guard<std::mutex> lg(_mutex);

      if(!FUSE::ReadDirFactory::valid(str_))
        return -EINVAL;

      _type = str_;
    }

  return 0;
}

/*
  Yeah... if not initialized it will crash... ¯\_(ツ)_/¯
  This will be resolved once initialization of internal objects and
  handling of input is better seperated.
*/
int
FUSE::ReadDir::operator()(fuse_file_info_t const *ffi_,
                          fuse_dirents_t         *buf_)
{
  std::shared_ptr<FUSE::ReadDirBase> readdir;

  {
    std::lock_guard<std::mutex> lg(_mutex);
    readdir = _readdir;
  }

  return (*readdir)(ffi_,buf_);
}

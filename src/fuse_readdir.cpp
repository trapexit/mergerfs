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

#include "fuse_readdir_factory.hpp"

#include "dirinfo.hpp"
#include "fuse_dirents.h"

#include "config.hpp"

#include <cstring>

#include <dirent.h>

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

static
int
_handle_ENOENT(const fuse_file_info_t *ffi_,
               fuse_dirents_t         *buf_)
{
  dirent de;
  DirInfo *di = reinterpret_cast<DirInfo*>(ffi_->fh);

  if(di->fusepath != "/")
    return -ENOENT;

  de = {0};

  de.d_ino = 0;
  de.d_off = 0;
  de.d_type = DT_REG;
  strcpy(de.d_name,"error: no valid mergerfs branch found, check config");
  de.d_reclen = sizeof(de);

  fuse_dirents_add(buf_,&de,::strlen(de.d_name));

  return 0;
}

/*
  Yeah... if not initialized it will crash... ¯\_(ツ)_/¯
  This will be resolved once initialization of internal objects and
  handling of input is better separated.
*/
int
FUSE::ReadDir::operator()(const fuse_file_info_t *ffi_,
                          fuse_dirents_t         *buf_)
{
  int rv;
  std::shared_ptr<FUSE::ReadDirBase> readdir;

  {
    std::lock_guard<std::mutex> lg(_mutex);
    readdir = _readdir;
  }

  rv = (*readdir)(ffi_,buf_);
  if(rv == -ENOENT)
    return ::_handle_ENOENT(ffi_,buf_);

  return rv;
}

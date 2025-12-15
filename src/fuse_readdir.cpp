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
#include "fuse_dirents.hpp"

#include "config.hpp"

#include <cstring>

#include <dirent.h>


int
FUSE::readdir(const fuse_req_ctx_t   *ctx_,
              const fuse_file_info_t *ffi_,
              fuse_dirents_t         *buf_)
{
  return cfg.readdir.read(ctx_,ffi_,buf_);
}

FUSE::ReadDir::ReadDir(const std::string_view s_)
  : _str()
{
  from_string(s_);
}

std::string
FUSE::ReadDir::to_string() const
{
  std::shared_ptr<std::string> str;

  str = std::atomic_load(&_str);

  return (*str);
}

int
FUSE::ReadDir::from_string(const std::string_view str_)
{
  std::shared_ptr<std::string> tmp_str;
  std::shared_ptr<FUSE::ReadDirBase> tmp_readdir;

  tmp_readdir = FUSE::ReadDirFactory::make(str_);
  if(!tmp_readdir)
    return -EINVAL;

  tmp_str = std::make_shared<std::string>(str_);
  std::atomic_store(&_str,tmp_str);

  if(_initialized == false)
    return 0;

  std::atomic_store(&_impl,tmp_readdir);

  return 0;
}

static
int
_handle_ENOENT(const fuse_file_info_t *ffi_,
               fuse_dirents_t         *buf_)
{
  dirent de;
  DirInfo *di = DirInfo::from_fh(ffi_->fh);

  if(!di->fusepath.empty())
    return -ENOENT;

  de = {};

  de.d_ino = 0;
  de.d_off = 0;
  de.d_type = DT_UNKNOWN;
  strcpy(de.d_name,"error: no valid mergerfs branch found, check config");
  de.d_reclen = sizeof(de);

  fuse_dirents_add(buf_,&de,::strlen(de.d_name));

  return 0;
}

int
FUSE::ReadDir::operator()(const fuse_req_ctx_t   *ctx_,
                          const fuse_file_info_t *ffi_,
                          fuse_dirents_t         *buf_)
{
  int rv;
  std::shared_ptr<FUSE::ReadDirBase> readdir;

  readdir = std::atomic_load(&_impl);
  assert(readdir);

  rv = (*readdir)(ctx_,ffi_,buf_);
  if(rv == -ENOENT)
    return ::_handle_ENOENT(ffi_,buf_);

  return rv;
}

void
FUSE::ReadDir::initialize()
{
  std::shared_ptr<std::string> str;

  str = std::atomic_load(&_str);
  _initialized = true;

  from_string(*str);
}

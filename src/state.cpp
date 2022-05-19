/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "state.hpp"

#define DEFAULT_LINK_EXDEV   LinkEXDEV::PASSTHROUGH
#define DEFAULT_RENAME_EXDEV RenameEXDEV::PASSTHROUGH

StateBase::Ptr g_STATE;


StateBase::StateBase(const toml::value &toml_)
  :
  branches(toml_),
  access(toml_),
  chmod(toml_),
  chown(toml_),
  create(toml_),
  getattr(toml_),
  getxattr(toml_),
  ioctl(toml_),
  link(toml_),
  listxattr(toml_),
  mkdir(toml_),
  mknod(toml_),
  open(toml_),
  readdir(toml_),
  readdirplus(toml_),
  readlink(toml_),
  removexattr(toml_),
  rename(toml_),
  rmdir(toml_),
  setxattr(toml_),
  statfs(toml_),
  symlink(toml_),
  truncate(toml_),
  unlink(toml_),
  utimens(toml_),
  write(toml_)
{
  mountpoint              = toml::find<std::string>(toml_,"filesystem","mountpoint");

  link_exdev              = toml::find_or(toml_,"func","link","exdev",DEFAULT_LINK_EXDEV);
  rename_exdev            = toml::find_or(toml_,"func","rename","exdev",DEFAULT_RENAME_EXDEV);

  entry_cache_timeout     = toml::find_or(toml_,"cache","entry-timeout",60);
  neg_entry_cache_timeout = toml::find_or(toml_,"cache","negative-entry-timeout",0);
  attr_cache_timeout      = toml::find_or(toml_,"cache","attr-timeout",60);
  writeback_cache         = toml::find_or(toml_,"cache","writeback",false);
  symlinkify              = toml::find_or(toml_,"func","symlinkify",false);
  symlinkify_timeout      = toml::find_or(toml_,"func","symlinkify-timeout",3600);
  security_capability     = toml::find_or(toml_,"xattr","security-capability",true);

  drop_cache_on_release   = toml::find_or(toml_,"func","release","drop-cache",false);
}

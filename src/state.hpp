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

#pragma once

#include "fuse_access_func.hpp"
#include "fuse_chmod_func.hpp"
#include "fuse_chown_func.hpp"
#include "fuse_create_func.hpp"
#include "fuse_getattr_func.hpp"
#include "fuse_getxattr_func.hpp"
#include "fuse_ioctl_func.hpp"
#include "fuse_link_func.hpp"
#include "fuse_listxattr_func.hpp"
#include "fuse_mkdir_func.hpp"
#include "fuse_mknod_func.hpp"
#include "fuse_open_func.hpp"
#include "fuse_readlink_func.hpp"
#include "fuse_removexattr_func.hpp"
#include "fuse_rename_func.hpp"
#include "fuse_rmdir_func.hpp"
#include "fuse_setxattr_func.hpp"
#include "fuse_symlink_func.hpp"
#include "fuse_truncate_func.hpp"
#include "fuse_unlink_func.hpp"

#include "branches.hpp"

#include "link_exdev_enum.hpp"
#include "rename_exdev_enum.hpp"

#include "ghc/filesystem.hpp"

#include <atomic>
#include <memory>

class StateBase;
class State;

extern std::shared_ptr<StateBase> g_STATE;

class StateBase
{
public:
  typedef std::shared_ptr<StateBase> Ptr;

public:
  StateBase(const toml::value &);

public:
  int entry_cache_timeout;
  int neg_entry_cache_timeout;
  int attr_cache_timeout;

public:
  bool     symlinkify;
  uint64_t symlinkify_timeout;

public:
  bool writeback_cache;

public:
  bool security_capability;

public:
  ghc::filesystem::path mountpoint;
  Branches2 branches;

public:
  LinkEXDEV   link_exdev;
  RenameEXDEV rename_exdev;

public:
  FUSE::ACCESS::Func      access;
  FUSE::CHMOD::Func       chmod;
  FUSE::CHOWN::Func       chown;
  FUSE::CREATE::Func      create;
  FUSE::GETATTR::Func     getattr;
  FUSE::GETXATTR::Func    getxattr;
  FUSE::IOCTL::Func       ioctl;
  FUSE::LINK::Func        link;
  FUSE::LISTXATTR::Func   listxattr;
  FUSE::MKDIR::Func       mkdir;
  FUSE::MKNOD::Func       mknod;
  FUSE::OPEN::Func        open;
  FUSE::READLINK::Func    readlink;
  FUSE::REMOVEXATTR::Func removexattr;
  FUSE::RENAME::Func      rename;
  FUSE::RMDIR::Func       rmdir;
  FUSE::SETXATTR::Func    setxattr;
  FUSE::SYMLINK::Func     symlink;
  FUSE::TRUNCATE::Func    truncate;
  FUSE::UNLINK::Func      unlink;

public:
  const toml::value _toml;
};

class State
{
public:
  State()
  {
    std::atomic_store(&_state,g_STATE);
  }

public:
  State&
  operator=(const toml::value &toml_)
  {
    StateBase::Ptr s;

    s = std::make_shared<StateBase>(toml_);

    std::atomic_store(&g_STATE,s);

    return *this;
  }

public:
  StateBase*
  operator->()
  {
    return _state.get();
  }

private:
  StateBase::Ptr _state;
};

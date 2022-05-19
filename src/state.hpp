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

#include "fuse_access_policy.hpp"
#include "fuse_chmod_policy.hpp"
#include "fuse_chown_policy.hpp"
#include "fuse_create_policy.hpp"
#include "fuse_getattr_policy.hpp"
#include "fuse_getxattr_policy.hpp"
#include "fuse_ioctl_policy.hpp"
#include "fuse_link_policy.hpp"
#include "fuse_listxattr_policy.hpp"
#include "fuse_mkdir_policy.hpp"
#include "fuse_mknod_policy.hpp"
#include "fuse_open_policy.hpp"
#include "fuse_readdir_policy.hpp"
#include "fuse_readlink_policy.hpp"
#include "fuse_removexattr_policy.hpp"
#include "fuse_rename_policy.hpp"
#include "fuse_rmdir_policy.hpp"
#include "fuse_setxattr_policy.hpp"
#include "fuse_symlink_policy.hpp"
#include "fuse_truncate_policy.hpp"
#include "fuse_unlink_policy.hpp"
#include "fuse_write_policy.hpp"

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
  bool drop_cache_on_release;

public:
  bool cache_readdir;

public:
  FUSE::ACCESS::Policy      access;
  FUSE::CHMOD::Policy       chmod;
  FUSE::CHOWN::Policy       chown;
  FUSE::CREATE::Policy      create;
  FUSE::GETATTR::Policy     getattr;
  FUSE::GETXATTR::Policy    getxattr;
  FUSE::IOCTL::Policy       ioctl;
  FUSE::LINK::Policy        link;
  FUSE::LISTXATTR::Policy   listxattr;
  FUSE::MKDIR::Policy       mkdir;
  FUSE::MKNOD::Policy       mknod;
  FUSE::OPEN::Policy        open;
  FUSE::READDIR::Policy     readdir;
  FUSE::READLINK::Policy    readlink;
  FUSE::REMOVEXATTR::Policy removexattr;
  FUSE::RENAME::Policy      rename;
  FUSE::RMDIR::Policy       rmdir;
  FUSE::SETXATTR::Policy    setxattr;
  FUSE::SYMLINK::Policy     symlink;
  FUSE::TRUNCATE::Policy    truncate;
  FUSE::UNLINK::Policy      unlink;
  FUSE::WRITE::Policy       write;


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

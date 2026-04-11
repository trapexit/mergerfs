/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_inode.hpp"

#include "rapidhash/rapidhash.h"

#include <atomic>
#include <sys/stat.h>

typedef u64 (*inodefunc_t)(const std::string_view,
                           const std::string_view,
                           const mode_t,
                           const ino_t);

static u64 _hybrid_hash(const std::string_view,
                        const std::string_view,
                        const mode_t,
                        const ino_t);


static std::atomic<inodefunc_t> g_func{::_hybrid_hash};


static
const void*
_data(const std::string_view s_)
{
  return (s_.empty() ? "" : s_.data());
}

static
u32
_h64_to_h32(u64 h_)
{
  h_ ^= (h_ >> 32);
  h_ *= 0x9E3779B9;
  return h_;
}

static
u64
_hash(const std::string_view s_)
{
  return rapidhash(::_data(s_),
                   s_.size());
}

static
u64
_branch_seed(const std::string_view branch_path_)
{
  return ::_hash(branch_path_);
}

static
u64
_devino_hash_from_seed(const u64   branch_seed_,
                       const ino_t ino_)
{
  return rapidhash_withSeed(&ino_,
                            sizeof(ino_),
                            branch_seed_);
}

static
u64
_path_hash_value(const std::string_view fusepath_)
{
  return ::_hash(fusepath_);
}

static
u64
_path_hash32_value(const std::string_view fusepath_)
{
  return ::_h64_to_h32(::_path_hash_value(fusepath_));
}

static
u64
_devino_hash_value(const std::string_view branch_path_,
                   const ino_t            ino_)
{
  return ::_devino_hash_from_seed(::_branch_seed(branch_path_),
                                  ino_);
}

static
u64
_devino_hash32_value(const std::string_view branch_path_,
                     const ino_t            ino_)
{
  return ::_h64_to_h32(::_devino_hash_value(branch_path_,ino_));
}

static
u64
_passthrough(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  return ino_;
}

static
u64
_path_hash(const std::string_view branch_path_,
           const std::string_view fusepath_,
           const mode_t           mode_,
           const ino_t            ino_)
{
  return ::_path_hash_value(fusepath_);
}

static
u64
_path_hash32(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  return ::_path_hash32_value(fusepath_);
}

static
u64
_devino_hash(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  return ::_devino_hash_value(branch_path_,ino_);
}

static
u64
_devino_hash32(const std::string_view branch_path_,
               const std::string_view fusepath_,
               const mode_t           mode_,
               const ino_t            ino_)
{
  return ::_devino_hash32_value(branch_path_,ino_);
}

static
u64
_hybrid_hash(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  return (S_ISDIR(mode_) ?
          ::_path_hash(branch_path_,fusepath_,mode_,ino_) :
          ::_devino_hash(branch_path_,fusepath_,mode_,ino_));
}

static
u64
_hybrid_hash32(const std::string_view branch_path_,
               const std::string_view fusepath_,
               const mode_t           mode_,
               const ino_t            ino_)
{
  return (S_ISDIR(mode_) ?
          ::_path_hash32(branch_path_,fusepath_,mode_,ino_) :
          ::_devino_hash32(branch_path_,fusepath_,mode_,ino_));
}

static
fs::inode::Algo
_algo_from_func(const inodefunc_t func_)
{
  if(func_ == ::_passthrough)
    return fs::inode::Algo::PASSTHROUGH;
  if(func_ == ::_path_hash)
    return fs::inode::Algo::PATH_HASH;
  if(func_ == ::_path_hash32)
    return fs::inode::Algo::PATH_HASH32;
  if(func_ == ::_devino_hash)
    return fs::inode::Algo::DEVINO_HASH;
  if(func_ == ::_devino_hash32)
    return fs::inode::Algo::DEVINO_HASH32;
  if(func_ == ::_hybrid_hash)
    return fs::inode::Algo::HYBRID_HASH;
  if(func_ == ::_hybrid_hash32)
    return fs::inode::Algo::HYBRID_HASH32;

  return fs::inode::Algo::HYBRID_HASH;
}

static
u64
_calc_with_algo(const fs::inode::Algo  algo_,
                const u64              branch_seed_,
                const std::string_view fusepath_,
                const mode_t           mode_,
                const ino_t            ino_)
{
  switch(algo_)
    {
    case fs::inode::Algo::PASSTHROUGH:
      return ino_;
    case fs::inode::Algo::PATH_HASH:
      return ::_path_hash_value(fusepath_);
    case fs::inode::Algo::PATH_HASH32:
      return ::_path_hash32_value(fusepath_);
    case fs::inode::Algo::DEVINO_HASH:
      return ::_devino_hash_from_seed(branch_seed_,ino_);
    case fs::inode::Algo::DEVINO_HASH32:
      return ::_h64_to_h32(::_devino_hash_from_seed(branch_seed_,ino_));
    case fs::inode::Algo::HYBRID_HASH:
      return (S_ISDIR(mode_) ?
              ::_path_hash_value(fusepath_) :
              ::_devino_hash_from_seed(branch_seed_,ino_));
    case fs::inode::Algo::HYBRID_HASH32:
      return (S_ISDIR(mode_) ?
              ::_path_hash32_value(fusepath_) :
              ::_h64_to_h32(::_devino_hash_from_seed(branch_seed_,ino_)));
    }

  return ino_;
}

int
fs::inode::set_algo(const std::string &algo_)
{
  inodefunc_t func = nullptr;

  if(algo_ == "passthrough")
    func = ::_passthrough;
  else if(algo_ == "path-hash")
    func = ::_path_hash;
  else if(algo_ == "path-hash32")
    func = ::_path_hash32;
  else if(algo_ == "devino-hash")
    func = ::_devino_hash;
  else if(algo_ == "devino-hash32")
    func = ::_devino_hash32;
  else if(algo_ == "hybrid-hash")
    func = ::_hybrid_hash;
  else if(algo_ == "hybrid-hash32")
    func = ::_hybrid_hash32;
  else
    return -EINVAL;

  g_func.store(func);

  return 0;
}

std::string
fs::inode::get_algo(void)
{
  switch(::_algo_from_func(g_func.load()))
    {
    case fs::inode::Algo::PASSTHROUGH:
      return "passthrough";
    case fs::inode::Algo::PATH_HASH:
      return "path-hash";
    case fs::inode::Algo::PATH_HASH32:
      return "path-hash32";
    case fs::inode::Algo::DEVINO_HASH:
      return "devino-hash";
    case fs::inode::Algo::DEVINO_HASH32:
      return "devino-hash32";
    case fs::inode::Algo::HYBRID_HASH:
      return "hybrid-hash";
    case fs::inode::Algo::HYBRID_HASH32:
      return "hybrid-hash32";
    }

  return {};
}

fs::inode::ReaddirCalc::ReaddirCalc(const fs::path &branch_path_,
                                    const fs::path &dirpath_)
  : _algo(::_algo_from_func(g_func.load())),
    _branch_seed(::_branch_seed(branch_path_.native())),
    _fusepath((dirpath_ / "__mergerfs__").native()),
    _filename_offset(_fusepath.size() - std::string_view("__mergerfs__").size())
{
}

u64
fs::inode::ReaddirCalc::calc(const char         *name_,
                             const std::size_t   namelen_,
                             const mode_t        mode_,
                             const ino_t         ino_)
{
  switch(_algo)
    {
    case Algo::PASSTHROUGH:
      return ino_;
    case Algo::DEVINO_HASH:
      return ::_devino_hash_from_seed(_branch_seed,ino_);
    case Algo::DEVINO_HASH32:
      return ::_h64_to_h32(::_devino_hash_from_seed(_branch_seed,ino_));
    case Algo::HYBRID_HASH:
      if(!S_ISDIR(mode_))
        return ::_devino_hash_from_seed(_branch_seed,ino_);
      break;
    case Algo::HYBRID_HASH32:
      if(!S_ISDIR(mode_))
        return ::_h64_to_h32(::_devino_hash_from_seed(_branch_seed,ino_));
      break;
    case Algo::PATH_HASH:
    case Algo::PATH_HASH32:
      break;
    }

  _fusepath.resize(_filename_offset);
  _fusepath.append(name_,namelen_);

  return ::_calc_with_algo(_algo,
                           _branch_seed,
                           _fusepath,
                           mode_,
                           ino_);
}

u64
fs::inode::calc(const std::string &branch_path_,
                const std::string &fusepath_,
                const mode_t       mode_,
                const ino_t        ino_)
{
  return g_func.load()(branch_path_,
                       fusepath_,
                       mode_,
                       ino_);
}

u64
fs::inode::calc(const fs::path &branch_path_,
                const fs::path &fusepath_,
                const mode_t    mode_,
                const ino_t     ino_)
{
  return g_func.load()(branch_path_.native(),
                       fusepath_.native(),
                       mode_,
                       ino_);
}

void
fs::inode::calc(const std::string &branch_path_,
                const std::string &fusepath_,
                struct stat       *st_)
{
  st_->st_ino = calc(branch_path_,
                     fusepath_,
                     st_->st_mode,
                     st_->st_ino);
}

void
fs::inode::calc(const fs::path &branch_path_,
                const fs::path &fusepath_,
                struct stat    *st_)
{
  st_->st_ino = calc(branch_path_,
                     fusepath_,
                     st_->st_mode,
                     st_->st_ino);
}

void
fs::inode::calc(const std::string &branch_path_,
                const std::string &fusepath_,
                struct fuse_statx *st_)
{
  st_->ino = calc(branch_path_,
                  fusepath_,
                  st_->mode,
                  st_->ino);
}

void
fs::inode::calc(const fs::path    &branch_path_,
                const fs::path    &fusepath_,
                struct fuse_statx *st_)
{
  st_->ino = calc(branch_path_,
                  fusepath_,
                  st_->mode,
                  st_->ino);
}

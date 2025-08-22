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

#include "fs_inode.hpp"

#include "ef.hpp"
#include "rapidhash.h"

#include <cstdint>

#include <pthread.h>
#include <sys/stat.h>

typedef uint64_t (*inodefunc_t)(const std::string_view,
                                const std::string_view,
                                const mode_t,
                                const ino_t);

static uint64_t _hybrid_hash(const std::string_view,
                             const std::string_view,
                             const mode_t,
                             const ino_t);


static inodefunc_t g_func = ::_hybrid_hash;


static
uint32_t
_h64_to_h32(uint64_t h_)
{
  h_ ^= (h_ >> 32);
  h_ *= 0x9E3779B9;
  return h_;
}

static
uint64_t
_passthrough(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  return ino_;
}

static
uint64_t
_path_hash(const std::string_view branch_path_,
           const std::string_view fusepath_,
           const mode_t           mode_,
           const ino_t            ino_)
{
  uint64_t seed;

  seed = rapidhash(&fusepath_[0],fusepath_.size());

  return seed;
}

static
uint64_t
_path_hash32(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  uint64_t h;

  h = ::_path_hash(branch_path_,
                   fusepath_,
                   mode_,
                   ino_);

  return ::_h64_to_h32(h);
}

static
uint64_t
_devino_hash(const std::string_view branch_path_,
             const std::string_view fusepath_,
             const mode_t           mode_,
             const ino_t            ino_)
{
  uint64_t seed;

  seed = rapidhash(&branch_path_[0],branch_path_.size());
  seed = rapidhash_withSeed(&ino_,sizeof(ino_),seed);

  return seed;
}

static
uint64_t
_devino_hash32(const std::string_view branch_path_,
               const std::string_view fusepath_,
               const mode_t           mode_,
               const ino_t            ino_)
{
  uint64_t h;

  h = ::_devino_hash(branch_path_,
                     fusepath_,
                     mode_,
                     ino_);

  return ::_h64_to_h32(h);
}

static
uint64_t
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
uint64_t
_hybrid_hash32(const std::string_view branch_path_,
               const std::string_view fusepath_,
               const mode_t           mode_,
               const ino_t            ino_)
{
  return (S_ISDIR(mode_) ?
          ::_path_hash32(branch_path_,fusepath_,mode_,ino_) :
          ::_devino_hash32(branch_path_,fusepath_,mode_,ino_));
}

int
fs::inode::set_algo(const std::string_view algo_)
{
  if(algo_ == "passthrough")
    g_func = ::_passthrough;
  ef(algo_ == "path-hash")
    g_func = ::_path_hash;
  ef(algo_ == "path-hash32")
    g_func = ::_path_hash32;
  ef(algo_ == "devino-hash")
    g_func = ::_devino_hash;
  ef(algo_ == "devino-hash32")
    g_func = ::_devino_hash32;
  ef(algo_ == "hybrid-hash")
    g_func = ::_hybrid_hash;
  ef(algo_ == "hybrid-hash32")
    g_func = ::_hybrid_hash32;
  else
    return -EINVAL;

  return 0;
}

std::string
fs::inode::get_algo(void)
{
  if(g_func == ::_passthrough)
    return "passthrough";
  if(g_func == ::_path_hash)
    return "path-hash";
  if(g_func == ::_path_hash32)
    return "path-hash32";
  if(g_func == ::_devino_hash)
    return "devino-hash";
  if(g_func == ::_devino_hash32)
    return "devino-hash32";
  if(g_func == ::_hybrid_hash)
    return "hybrid-hash";
  if(g_func == ::_hybrid_hash32)
    return "hybrid-hash32";

  return {};
}

uint64_t
fs::inode::calc(const std::string_view branch_path_,
                const std::string_view fusepath_,
                const mode_t           mode_,
                const ino_t            ino_)
{
  return g_func(branch_path_,fusepath_,mode_,ino_);
}

void
fs::inode::calc(const std::string_view  branch_path_,
                const std::string_view  fusepath_,
                struct stat            *st_)
{
  st_->st_ino = calc(branch_path_,
                     fusepath_,
                     st_->st_mode,
                     st_->st_ino);
}

void
fs::inode::calc(const std::string_view  branch_path_,
                const std::string_view  fusepath_,
                struct fuse_statx      *st_)
{
  st_->ino = calc(branch_path_,
                  fusepath_,
                  st_->mode,
                  st_->ino);
}

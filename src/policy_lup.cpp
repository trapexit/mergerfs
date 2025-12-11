/*
  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link> and contributors

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

#include "policy_lup.hpp"

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "policies.hpp"
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "rwlock.hpp"

#include <limits>
#include <string>

using std::string;

static int
_action(const Branches::Ptr &branches_,
        const fs::path &fusepath_,
        std::vector<Branch *> &paths_)
{
  int rv;
  int error;
  fs::info_t info;
  Branch *obranch;

  obranch = nullptr;
  error = ENOENT;

  uint64_t best_used = 0;
  uint64_t best_total = 1;

  for (auto &branch : *branches_)
  {
    if (branch.ro())
      error_and_continue(error, EROFS);
    if (!fs::exists(branch.path, fusepath_))
      error_and_continue(error, ENOENT);
    rv = fs::info(branch.path, &info);
    if (rv < 0)
      error_and_continue(error, ENOENT);
    if (info.readonly)
      error_and_continue(error, EROFS);

    uint64_t used = info.spaceused;
    uint64_t total = info.spaceused + info.spaceavail;
    if (total == 0)
    {
      used = 0;
      total = 1;
    }

    if (obranch == nullptr)
    {
      best_used = used;
      best_total = total;
      obranch = &branch;
      continue;
    }

    unsigned __int128 lhs = (unsigned __int128)used * (unsigned __int128)best_total;
    unsigned __int128 rhs = (unsigned __int128)best_used * (unsigned __int128)total;
    if (lhs >= rhs)
      continue;

    best_used = used;
    best_total = total;
    obranch = &branch;
  }

  if (obranch == nullptr)
    return -error;

  paths_.push_back(obranch);

  return 0;
}

static int
_search(const Branches::Ptr &branches_,
        const fs::path &fusepath_,
        std::vector<Branch *> &paths_)
{
  int rv;
  uint64_t used;
  uint64_t avail;
  uint64_t best_used = 0;
  uint64_t best_total = 1;
  Branch *obranch;

  obranch = nullptr;

  for (auto &branch : *branches_)
  {
    if (!fs::exists(branch.path, fusepath_))
      continue;
    rv = fs::statvfs_cache_spaceused(branch.path, &used);
    if (rv < 0)
      continue;
    rv = fs::statvfs_cache_spaceavail(branch.path, &avail);
    if (rv < 0)
      continue;

    uint64_t total = used + avail;
    if (total == 0)
    {
      used = 0;
      total = 1;
    }

    if (obranch == nullptr)
    {
      best_used = used;
      best_total = total;
      obranch = &branch;
      continue;
    }

    unsigned __int128 lhs = (unsigned __int128)used * (unsigned __int128)best_total;
    unsigned __int128 rhs = (unsigned __int128)best_used * (unsigned __int128)total;
    if (lhs >= rhs)
      continue;

    best_used = used;
    best_total = total;
    obranch = &branch;
  }

  if (obranch == nullptr)
    return -ENOENT;

  paths_.push_back(obranch);

  return 0;
}

static int
_create(const Branches::Ptr &branches_,
        std::vector<Branch *> &paths_)
{
  int rv;
  int error;
  fs::info_t info;
  Branch *obranch;

  obranch = nullptr;
  error = ENOENT;

  uint64_t best_used = 0;
  uint64_t best_total = 1;

  for (auto &branch : *branches_)
  {
    if (branch.ro_or_nc())
      error_and_continue(error, EROFS);
    rv = fs::info(branch.path, &info);
    if (rv < 0)
      error_and_continue(error, ENOENT);
    if (info.readonly)
      error_and_continue(error, EROFS);
    if (info.spaceavail < branch.minfreespace())
      error_and_continue(error, ENOSPC);

    uint64_t used = info.spaceused;
    uint64_t total = info.spaceused + info.spaceavail;
    if (total == 0)
    {
      used = 0;
      total = 1;
    }

    if (obranch == nullptr)
    {
      best_used = used;
      best_total = total;
      obranch = &branch;
      continue;
    }

    unsigned __int128 lhs = (unsigned __int128)used * (unsigned __int128)best_total;
    unsigned __int128 rhs = (unsigned __int128)best_used * (unsigned __int128)total;
    if (lhs >= rhs)
      continue;

    best_used = used;
    best_total = total;
    obranch = &branch;
  }

  if (obranch == nullptr)
    return -error;

  paths_.push_back(obranch);

  return 0;
}

int Policy::LUP::Action::operator()(const Branches::Ptr &branches_,
                                    const fs::path &fusepath_,
                                    std::vector<Branch *> &paths_) const
{
  return ::_action(branches_, fusepath_, paths_);
}

int Policy::LUP::Create::operator()(const Branches::Ptr &branches_,
                                    const fs::path &fusepath_,
                                    std::vector<Branch *> &paths_) const
{
  return ::_create(branches_, paths_);
}

int Policy::LUP::Search::operator()(const Branches::Ptr &branches_,
                                    const fs::path &fusepath_,
                                    std::vector<Branch *> &paths_) const
{
  return ::_search(branches_, fusepath_, paths_);
}

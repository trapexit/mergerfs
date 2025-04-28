/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_wait_for_mount.hpp"
#include "syslog.hpp"

#include "fs_exists.hpp"
#include "fs_lstat.hpp"
#include "fs_lgetxattr.hpp"

#include <functional>
#include <thread>
#include <unordered_set>


namespace fs
{
  typedef std::unordered_set<fs::Path> PathSet;
}

constexpr std::chrono::milliseconds SLEEP_DURATION = std::chrono::milliseconds(333);

namespace std
{
  template<>
  struct hash<fs::Path>
  {
    std::size_t
    operator()(fs::Path const &path_) const noexcept
    {
      return std::hash<std::string>{}(path_.string());
    }
  };
}

static
bool
_branch_is_mounted(const struct stat &src_st_,
                   const fs::Path    &branch_path_)
{
  int rv;
  struct stat st;
  fs::Path filepath;

  rv = fs::lgetxattr(branch_path_,"user.mergerfs.branch",NULL,0);
  if(rv != -1)
    return true;

  filepath = branch_path_ / ".mergerfs.branch";
  rv = fs::exists(filepath);
  if(rv)
    return true;

  rv = fs::lgetxattr(branch_path_,"user.mergerfs.branch_mounts_here",NULL,0);
  if(rv != -1)
    return false;

  filepath = branch_path_ / ".mergerfs.branch_mounts_here";
  rv = fs::exists(filepath);
  if(rv)
    return false;

  rv = fs::lstat(branch_path_,&st);
  if(rv == 0)
    return (st.st_dev != src_st_.st_dev);

  return false;
}

static
void
_check_mounted(const struct stat &src_st_,
               const fs::PathSet &tgt_paths_,
               fs::PathVector    *successes_,
               fs::PathVector    *failures_)
{
  fs::PathVector &successes = *successes_;
  fs::PathVector &failures  = *failures_;

  for(auto const &tgt_path : tgt_paths_)
    {
      bool mounted;

      mounted = ::_branch_is_mounted(src_st_,tgt_path);
      if(mounted)
        successes.push_back(tgt_path);
      else
        failures.push_back(tgt_path);
    }
}

static
int
_wait_for_mount(const struct stat               &src_st_,
                const fs::PathVector            &tgt_paths_,
                const std::chrono::milliseconds &timeout_)
{
  bool first_loop;
  fs::PathVector                                     successes;
  fs::PathVector                                     failures;
  std::unordered_set<fs::Path>                       tgt_paths;
  std::chrono::time_point<std::chrono::steady_clock> now;
  std::chrono::time_point<std::chrono::steady_clock> deadline;

  tgt_paths.insert(tgt_paths_.begin(),tgt_paths_.end());
  now      = std::chrono::steady_clock::now();
  deadline = now + timeout_;

  first_loop = true;
  while(true)
    {
      if(tgt_paths.empty())
        break;
      if(now >= deadline)
        break;

      successes.clear();
      failures.clear();
      ::_check_mounted(src_st_,tgt_paths,&successes,&failures);
      for(const auto &path : successes)
        {
          tgt_paths.erase(path);
          SysLog::info("{} is mounted",path.string());
        }

      if(first_loop)
        {
          for(const auto &path : failures)
            SysLog::notice("{} is not mounted, waiting",path.string());
          first_loop = false;
        }

      std::this_thread::sleep_for(SLEEP_DURATION);
      now = std::chrono::steady_clock::now();
    }

  for(auto const &path : failures)
    SysLog::notice("{} not mounted within timeout",path.string());

  return failures.size();
}

int
fs::wait_for_mount(const fs::Path                  &src_path_,
                   const fs::PathVector            &tgt_paths_,
                   const std::chrono::milliseconds &timeout_)
{
  int rv;
  struct stat src_st = {0};

  rv = fs::stat(src_path_,&src_st);
  if(rv == -1)
    SysLog::error("Error stat'ing mount path: {} ({})",
                  src_path_.string(),
                  strerror(errno));

  return ::_wait_for_mount(src_st,tgt_paths_,timeout_);
}

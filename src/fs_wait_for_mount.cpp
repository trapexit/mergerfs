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
      int         rv;
      struct stat tgt_st;

      rv = fs::stat(tgt_path,&tgt_st);
      if(rv == 0)
        {
          if(tgt_st.st_dev != src_st_.st_dev)
            successes.push_back(tgt_path);
          else
            failures.push_back(tgt_path);
        }
      else
        {
          failures.push_back(tgt_path);
        }
    }
}

static
void
_wait_for_mount(const struct stat               &src_st_,
                const fs::PathVector            &tgt_paths_,
                const std::chrono::milliseconds &timeout_)
{
  fs::PathVector                                     successes;
  fs::PathVector                                     failures;
  std::unordered_set<fs::Path>                       tgt_paths;
  std::chrono::time_point<std::chrono::steady_clock> now;
  std::chrono::time_point<std::chrono::steady_clock> deadline;

  tgt_paths.insert(tgt_paths_.begin(),tgt_paths_.end());
  now      = std::chrono::steady_clock::now();
  deadline = now + timeout_;

  while(true)
    {
      if(tgt_paths.empty())
        break;
      if(now >= deadline)
        break;

      successes.clear();
      failures.clear();
      ::_check_mounted(src_st_,tgt_paths,&successes,&failures);
      for(auto const &path : successes)
        {
          tgt_paths.erase(path);
          syslog_info("%s is mounted",path.string().c_str());
        }

      std::this_thread::sleep_for(SLEEP_DURATION);
      now = std::chrono::steady_clock::now();
    }

  for(auto const &path : failures)
    syslog_notice("%s not mounted within timeout",path.string().c_str());
  if(!failures.empty())
    syslog_warning("Continuing to mount mergerfs despite %u branches not "
                   "being different from the mountpoint filesystem",
                   failures.size());
}

void
fs::wait_for_mount(const fs::Path                  &src_path_,
                   const fs::PathVector            &tgt_paths_,
                   const std::chrono::milliseconds &timeout_)
{
  int rv;
  struct stat src_st;

  rv = fs::stat(src_path_,&src_st);
  if(rv == -1)
    return syslog_error("Error stat'ing mount path: %s (%s)",
                        src_path_.c_str(),
                        strerror(errno));

  ::_wait_for_mount(src_st,tgt_paths_,timeout_);
}

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

#include <thread>

constexpr std::chrono::milliseconds SLEEP_DURATION = std::chrono::milliseconds(333);


bool
fs::wait_for_mount(const struct stat               &src_st_,
                   const ghc::filesystem::path     &tgtpath_,
                   const std::chrono::milliseconds &timeout_)
{
  int rv;
  std::chrono::duration<double> time_diff;
  std::chrono::time_point<std::chrono::steady_clock> start_time;

  start_time = std::chrono::steady_clock::now();
  while(true)
    {
      struct stat tgt_st;

      rv = fs::stat(tgtpath_,&tgt_st);
      if(rv == 0)
        {
          if(tgt_st.st_dev != src_st_.st_dev)
            return true;
        }

      time_diff = (std::chrono::steady_clock::now() - start_time);
      if(time_diff > timeout_)
        return false;

      std::this_thread::sleep_for(SLEEP_DURATION);
    }

  return false;
}

static
void
_wait_for_mount(const struct stat               &src_st_,
                const fs::PathVector            &tgtpaths_,
                const std::chrono::milliseconds &timeout_,
                fs::PathVector                  &failed_paths_)
{
  bool rv;
  fs::PathVector::const_iterator i;
  std::chrono::milliseconds timeout;
  std::chrono::milliseconds diff;
  std::chrono::time_point<std::chrono::steady_clock> now;
  std::chrono::time_point<std::chrono::steady_clock> start_time;

  timeout = timeout_;
  now = start_time = std::chrono::steady_clock::now();
  for(auto i = tgtpaths_.begin(); i != tgtpaths_.end(); ++i)
    {
      diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
      timeout -= diff;

      rv = fs::wait_for_mount(src_st_,*i,timeout);
      if(rv == false)
        failed_paths_.push_back(*i);

      now = std::chrono::steady_clock::now();
    }
}

void
fs::wait_for_mount(const struct stat               &src_st_,
                   const fs::PathVector            &tgtpaths_,
                   const std::chrono::milliseconds &timeout_,
                   fs::PathVector                  &failed_paths_)
{
  if(tgtpaths_.empty())
    return;

  _wait_for_mount(src_st_,tgtpaths_,timeout_,failed_paths_);
}

void
fs::wait_for_mount(const ghc::filesystem::path     &srcpath_,
                   const fs::PathVector            &tgtpaths_,
                   const std::chrono::milliseconds &timeout_,
                   fs::PathVector                  &failed_paths_)
{
  int rv;
  struct stat src_st;

  rv = fs::stat(srcpath_,&src_st);
  if(rv == -1)
    return;

  fs::wait_for_mount(src_st,tgtpaths_,timeout_,failed_paths_);
}

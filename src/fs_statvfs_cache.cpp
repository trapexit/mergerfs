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

#include "fs_statvfs_cache.hpp"

#include "fs_statvfs.hpp"
#include "statvfs_util.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <sys/statvfs.h>


// The result of the last statvfs: the data, or the negative errno it
// failed with. `valid` is false until the first statvfs has run, so a
// freshly inserted entry is never taken for a fresh result whatever the
// timeout.
struct Element
{
  u64            time  = 0;
  bool           valid = false;
  int            rv    = 0;
  struct statvfs st    = {};
};

typedef std::unordered_map<std::string,Element> statvfs_cache;

// Cache is read heavy. Use a shared mutex so concurrent reads can
// work in parallel and only inserts or refreshes take a unique lock.
// The timeout is changed at runtime (cache.statfs) while requests
// read it.
static std::shared_mutex g_cache_mutex;
static std::atomic<u64>  g_timeout{0};
static statvfs_cache     g_cache;


static
u64
_get_time(void)
{
  using namespace std::chrono;
  return duration_cast<seconds>(steady_clock::now().time_since_epoch()).count();
}

u64
fs::statvfs_cache_timeout(void)
{
  return g_timeout.load(std::memory_order_relaxed);
}

void
fs::statvfs_cache_timeout(cu64 timeout_)
{
  g_timeout.store(timeout_,std::memory_order_relaxed);
}

static
bool
_fresh(const Element &e_,
       const u64      now_,
       const u64      timeout_)
{
  return (e_.valid && ((now_ - e_.time) <= timeout_));
}

static
int
_result(const Element  &e_,
        struct statvfs *st_)
{
  if(e_.rv < 0)
    return e_.rv;

  *st_ = e_.st;

  return 0;
}

int
fs::statvfs_cache(const std::string &path_,
                  struct statvfs    *st_)
{
  u64 timeout;

  timeout = g_timeout.load(std::memory_order_relaxed);
  if(timeout == 0)
    return fs::statvfs(path_,st_);

  {
    std::shared_lock<std::shared_mutex> lk(g_cache_mutex);

    auto it = g_cache.find(path_);
    if((it != g_cache.end()) && ::_fresh(it->second,::_get_time(),timeout))
      return ::_result(it->second,st_);
  }

  std::unique_lock<std::shared_mutex> lk(g_cache_mutex);

  // Another caller may have refreshed the entry while we waited.
  Element &e = g_cache[path_];
  if(not ::_fresh(e,::_get_time(),timeout))
    {
      // A failure is kept for the timeout like a success. Retrying it on
      // every call would hold this lock for a slow failing branch,
      // such as an unreachable network mount, one caller at a time.
      // It is never stored as zeroed space.
      e.rv    = fs::statvfs(path_,&e.st);
      e.time  = ::_get_time();
      e.valid = true;
    }

  return ::_result(e,st_);
}

int
fs::statvfs_cache_readonly(const std::string &path_,
                           bool              *readonly_)
{
  int rv;
  struct statvfs st;

  rv = fs::statvfs_cache(path_,&st);
  if(rv == 0)
    *readonly_ = StatVFS::readonly(st);

  return rv;
}

int
fs::statvfs_cache_spaceavail(const std::string &path_,
                             u64               *spaceavail_)
{
  int rv;
  struct statvfs st;

  rv = fs::statvfs_cache(path_,&st);
  if(rv == 0)
    *spaceavail_ = StatVFS::spaceavail(st);

  return rv;
}

int
fs::statvfs_cache_spaceused(const std::string &path_,
                            u64               *spaceused_)
{
  int rv;
  struct statvfs st;

  rv = fs::statvfs_cache(path_,&st);
  if(rv == 0)
    *spaceused_ = StatVFS::spaceused(st);

  return rv;
}

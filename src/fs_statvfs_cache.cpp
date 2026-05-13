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

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <sys/statvfs.h>
#include <time.h>


struct Element
{
  u64            time;
  struct statvfs st;
};

typedef std::unordered_map<std::string,Element> statvfs_cache;

// Cache is read heavy. Use a shared mutex so concurrent reads can
// work in parallel and only inserts or refreshes take a unique lock.
static std::shared_mutex g_cache_mutex;
static u64               g_timeout = 0;
static statvfs_cache     g_cache;


static
u64
_get_time(void)
{
  u64 rv;

  rv = ::time(NULL);

  return rv;
}

u64
fs::statvfs_cache_timeout(void)
{
  return g_timeout;
}

void
fs::statvfs_cache_timeout(cu64 timeout_)
{
  g_timeout = timeout_;
}

int
fs::statvfs_cache(const std::string &path_,
                  struct statvfs    *st_)
{
  u64 now;

  if(g_timeout == 0)
    return fs::statvfs(path_,st_);

  now = ::_get_time();

  {
    std::shared_lock<std::shared_mutex> lk(g_cache_mutex);

    auto it = g_cache.find(path_);
    if((it != g_cache.end()) && ((now - it->second.time) <= g_timeout))
      {
        *st_ = it->second.st;
        return 0;
      }
  }

  int rv = 0;
  std::unique_lock<std::shared_mutex> lk(g_cache_mutex);

  Element &e = g_cache[path_];
  if((now - e.time) > g_timeout)
    {
      e.time = now;
      rv = fs::statvfs(path_,&e.st);
    }

  *st_ = e.st;

  return rv;
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

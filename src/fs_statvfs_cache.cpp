/*
  ISC License

  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_statvfs.hpp"
#include "statvfs_util.hpp"

#include <cstdint>
#include <map>
#include <string>

#include <pthread.h>
#include <sys/statvfs.h>
#include <time.h>


struct Element
{
  uint64_t       time;
  struct statvfs st;
};

typedef std::map<std::string,Element> statvfs_cache;

static uint64_t        g_timeout    = 0;
static statvfs_cache   g_cache;
static pthread_mutex_t g_cache_lock = PTHREAD_MUTEX_INITIALIZER;

namespace l
{
  static
  uint64_t
  get_time(void)
  {
    uint64_t rv;

    rv = ::time(NULL);

    return rv;
  }
}

namespace fs
{
  uint64_t
  statvfs_cache_timeout(void)
  {
    return g_timeout;
  }

  void
  statvfs_cache_timeout(const uint64_t timeout_)
  {
    g_timeout = timeout_;
  }

  int
  statvfs_cache(const char     *path_,
                struct statvfs *st_)
  {
    int rv;
    Element *e;
    uint64_t now;

    if(g_timeout == 0)
      return fs::statvfs(path_,st_);

    rv = 0;
    now = l::get_time();

    pthread_mutex_lock(&g_cache_lock);

    e = &g_cache[path_];

    if((now - e->time) > g_timeout)
      {
        e->time = now;
        rv = fs::statvfs(path_,&e->st);
      }

    *st_ = e->st;

    pthread_mutex_unlock(&g_cache_lock);

    return rv;
  }

  int
  statvfs_cache_readonly(const std::string &path_,
                         bool              *readonly_)
  {
    int rv;
    struct statvfs st;

    rv = fs::statvfs_cache(path_.c_str(),&st);
    if(rv == 0)
      *readonly_ = StatVFS::readonly(st);

    return rv;
  }

  int
  statvfs_cache_spaceavail(const std::string &path_,
                           uint64_t          *spaceavail_)
  {
    int rv;
    struct statvfs st;

    rv = fs::statvfs_cache(path_.c_str(),&st);
    if(rv == 0)
      *spaceavail_ = StatVFS::spaceavail(st);

    return rv;
  }

  int
  statvfs_cache_spaceused(const std::string &path_,
                          uint64_t          *spaceused_)
  {
    int rv;
    struct statvfs st;

    rv = fs::statvfs_cache(path_.c_str(),&st);
    if(rv == 0)
      *spaceused_ = StatVFS::spaceused(st);

    return rv;
  }
}

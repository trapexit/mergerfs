/*
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

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <vector>

#define MAXGIDS 32
#define MAXRECS 256

// GIDCache is a global, per thread cache of uid to gid + supplemental
// groups mapping for use when threads change credentials. This is
// needed due to the high cost of querying such information. The cache
// instance should always be thread local and live the lifetime of the
// app. The constructor will register the instance so they can each be
// told to invalidate the cache on demand. A second instance on the
// same thread will cause an assert to be triggered.

struct GIDRecord
{
  std::vector<gid_t> gids;
  time_t last_update;
};

struct GIDCache
{
public:
  static
  int
  initgroups(const uid_t uid,
             const gid_t gid);

  static void invalidate_all();
  static void clear_all();
  static void clear_unused();

public:
  static int expire_timeout;
  static int remove_timeout;

private:
  static boost::concurrent_flat_map<uid_t,GIDRecord> _records;
};

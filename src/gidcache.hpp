/*
  The MIT License (MIT)

  Copyright (c) 2015 Antonio SJ Musumeci <trapexit@spawn.link>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef __GIDCACHE_HPP__
#define __GIDCACHE_HPP__

#include <sys/types.h>
#include <unistd.h>

#define MAXGIDS 32
#define MAXRECS 256

struct gid_t_rec
{
  uid_t uid;
  int   size;
  gid_t gids[MAXGIDS];

  bool
  operator<(const struct gid_t_rec &b) const;
};

struct gid_t_cache
{
public:
  size_t     size;
  gid_t_rec  recs[MAXRECS];

private:
  gid_t_rec * begin(void);
  gid_t_rec * end(void);
  gid_t_rec * allocrec(void);
  gid_t_rec * lower_bound(gid_t_rec   *begin,
                          gid_t_rec   *end,
                          const uid_t  uid);
  gid_t_rec * cache(const uid_t uid,
                    const gid_t gid);

public:
  int
  initgroups(const uid_t uid,
             const gid_t gid);
};

#endif

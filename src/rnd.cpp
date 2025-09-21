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

#include "rnd.hpp"

#include "rapidhash.h"

#include <cstdint>

#include <sys/time.h>


static uint64_t G_SEED;

__attribute__((constructor))
static
void
_constructor()
{
  struct timeval tv;

  gettimeofday(&tv,NULL);

  G_SEED   = tv.tv_sec;
  G_SEED <<= 32;
  G_SEED  |= tv.tv_usec;
}

// Lifted from wyhash.h's wyrand()
static
uint64_t
_rapidhash_rand(uint64_t *seed_)
{
  *seed_ += 0x2d358dccaa6c78a5ull;
  return rapid_mix(*seed_,*seed_ ^ 0x8bb84b93962eacc9ull);
}

uint64_t
RND::rand64(void)
{
  return _rapidhash_rand(&G_SEED);
}

uint64_t
RND::rand64(const uint64_t max_)
{
  return (RND::rand64() % max_);
}

uint64_t
RND::rand64(const uint64_t min_,
            const uint64_t max_)
{
  return (min_ + (RND::rand64() % (max_ - min_)));
}

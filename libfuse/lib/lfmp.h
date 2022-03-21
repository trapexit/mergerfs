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

#pragma once

#include "fmp.h"

#include <pthread.h>


typedef struct lfmp_t lfmp_t;
struct lfmp_t
{
  fmp_t fmp;
  pthread_mutex_t lock;
};


static
inline
void
lfmp_init(lfmp_t         *lfmp_,
          const uint64_t  obj_size_,
          const uint64_t  page_multiple_)
{
  fmp_init(&lfmp_->fmp,obj_size_,page_multiple_);
  pthread_mutex_init(&lfmp_->lock,NULL);
}

static
inline
void
lfmp_lock(lfmp_t *lfmp_)
{
  pthread_mutex_lock(&lfmp_->lock);
}

static
inline
void
lfmp_unlock(lfmp_t *lfmp_)
{
  pthread_mutex_unlock(&lfmp_->lock);
}

static
inline
uint64_t
lfmp_slab_count(lfmp_t *lfmp_)
{
  uint64_t rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_slab_count(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
int
lfmp_slab_alloc(lfmp_t *lfmp_)
{
  int rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_slab_alloc(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
void*
lfmp_alloc(lfmp_t *lfmp_)
{
  void *rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_alloc(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
void*
lfmp_calloc(lfmp_t *lfmp_)
{
  void *rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_calloc(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
void
lfmp_free(lfmp_t *lfmp_,
          void   *obj_)
{
  pthread_mutex_lock(&lfmp_->lock);
  fmp_free(&lfmp_->fmp,obj_);
  pthread_mutex_unlock(&lfmp_->lock);
}

static
inline
void
lfmp_clear(lfmp_t *lfmp_)
{
  pthread_mutex_lock(&lfmp_->lock);
  fmp_clear(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);
}

static
inline
void
lfmp_destroy(lfmp_t *lfmp_)
{
  pthread_mutex_lock(&lfmp_->lock);
  fmp_destroy(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);
  pthread_mutex_destroy(&lfmp_->lock);
}

static
inline
uint64_t
lfmp_avail_objs(lfmp_t *lfmp_)
{
  uint64_t rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_avail_objs(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
uint64_t
lfmp_objs_in_slab(lfmp_t *lfmp_,
                  void   *slab_)
{
  uint64_t rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_objs_in_slab(&lfmp_->fmp,slab_);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
void
lfmp_remove_objs_in_slab(lfmp_t *lfmp_,
                         void   *slab_)
{
  pthread_mutex_lock(&lfmp_->lock);
  fmp_remove_objs_in_slab(&lfmp_->fmp,slab_);
  pthread_mutex_unlock(&lfmp_->lock);
}

static
inline
int
lfmp_gc(lfmp_t *lfmp_)
{
  int rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_gc(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
uint64_t
lfmp_objs_per_slab(lfmp_t *lfmp_)
{
  uint64_t rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_objs_per_slab(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
double
lfmp_slab_usage_ratio(lfmp_t *lfmp_)
{
  double rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_slab_usage_ratio(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

static
inline
uint64_t
lfmp_total_allocated_memory(lfmp_t *lfmp_)
{
  uint64_t rv;

  pthread_mutex_lock(&lfmp_->lock);
  rv = fmp_total_allocated_memory(&lfmp_->fmp);
  pthread_mutex_unlock(&lfmp_->lock);

  return rv;
}

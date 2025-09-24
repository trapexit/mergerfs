#define _FILE_OFFSET_BITS 64

#include "fs_dirent64.hpp"
#include "fuse_attr.h"
#include "fuse_dirent.h"
#include "fuse_direntplus.h"
#include "fuse_dirents.hpp"
#include "fuse_entry.h"
#include "stat_utils.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* 32KB - same as glibc getdents buffer size */
#define DENTS_BUF_EXPAND_SIZE (1024 * 32)

static
uint64_t
_round_up(const uint64_t number_,
          const uint64_t multiple_)
{
  return (((number_ + multiple_ - 1) / multiple_) * multiple_);
}

static
uint64_t
_align_uint64_t(uint64_t v_)
{
  return ((v_ + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));
}

static
uint64_t
_dirent_size(const uint64_t namelen_)
{
  uint64_t rv;

  rv  = offsetof(fuse_dirent_t,name);
  rv += namelen_;
  rv  = _align_uint64_t(rv);

  return rv;
}

static
int
_dirents_buf_resize(fuse_dirents_t *d_,
                    const uint64_t  size_)
{
  if((kv_size(d_->data) + size_) >= kv_max(d_->data))
    {
      uint64_t new_size;

      new_size = _round_up((kv_size(d_->data) + size_),DENTS_BUF_EXPAND_SIZE);

      kv_resize(char,d_->data,new_size);
      if(d_->data.a == NULL)
        return -ENOMEM;
    }

  return 0;
}

static
fuse_dirent_t*
_dirents_dirent_alloc(fuse_dirents_t *d_,
                      const uint64_t  namelen_)
{
  int rv;
  uint64_t size;
  fuse_dirent_t *d;

  size = _dirent_size(namelen_);

  rv = _dirents_buf_resize(d_,size);
  if(rv)
    return NULL;

  d = (fuse_dirent_t*)&kv_end(d_->data);
  kv_size(d_->data) += size;

  return d;
}

int
fuse_dirents_add(fuse_dirents_t *d_,
                 const dirent   *de_,
                 const uint64_t  namelen_)
{
  fuse_dirent_t *d;

  d = _dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,kv_size(d_->data));
  d->ino     = de_->d_ino;
  d->namelen = namelen_;
  d->type    = de_->d_type;
  memcpy(d->name,de_->d_name,namelen_);

  return 0;
}

int
fuse_dirents_add(fuse_dirents_t     *d_,
                 const fs::dirent64 *de_,
                 const uint64_t      namelen_)
{
  fuse_dirent_t *d;

  d = _dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,kv_size(d_->data));
  d->ino     = de_->d_ino;
  d->namelen = namelen_;
  d->type    = de_->d_type;
  memcpy(d->name,de_->d_name,namelen_);

  return 0;
}

void
fuse_dirents_reset(fuse_dirents_t *d_)
{
  kv_size(d_->data) = 0;
  kv_size(d_->offs) = 1;
}

int
fuse_dirents_init(fuse_dirents_t *d_)
{
  kv_init(d_->data);
  kv_resize(char,d_->data,DENTS_BUF_EXPAND_SIZE);
  if(d_->data.a == NULL)
    return -ENOMEM;

  kv_init(d_->offs);
  kv_resize(uint32_t,d_->offs,64);
  kv_push(uint32_t,d_->offs,0);

  return 0;
}

void
fuse_dirents_free(fuse_dirents_t *d_)
{
  kv_destroy(d_->data);
  kv_destroy(d_->offs);
}

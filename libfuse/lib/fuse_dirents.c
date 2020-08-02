#define _FILE_OFFSET_BITS 64

#include "fuse_attr.h"
#include "fuse_dirent.h"
#include "fuse_direntplus.h"
#include "fuse_dirents.h"
#include "fuse_entry.h"
#include "linux_dirent64.h"
#include "stat_utils.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 32KB - same as glibc getdents buffer size */
#define DEFAULT_SIZE (1024 * 32)

static
uint64_t
align_uint64_t(uint64_t v_)
{
  return ((v_ + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));
}

static
uint64_t
fuse_dirent_size(const uint64_t namelen_)
{
  uint64_t rv;

  rv  = offsetof(fuse_dirent_t,name);
  rv += namelen_;
  rv  = align_uint64_t(rv);

  return rv;
}

static
uint64_t
fuse_direntplus_size(const uint64_t namelen_)
{
  uint64_t rv;

  rv  = offsetof(fuse_direntplus_t,dirent.name);
  rv += namelen_;
  rv  = align_uint64_t(rv);

  return rv;
}

static
int
fuse_dirents_buf_resize(fuse_dirents_t *d_,
                        uint64_t        size_)
{
  void *p;

  if((d_->data_len + size_) >= d_->buf_len)
    {
      p = realloc(d_->buf,(d_->buf_len * 2));
      if(p == NULL)
        return -errno;

      d_->buf      = p;
      d_->buf_len *= 2;
    }

  return 0;
}

static
void*
fuse_dirents_dirent_alloc(fuse_dirents_t *d_,
                          uint64_t        namelen_)
{
  int rv;
  uint64_t size;
  fuse_dirent_t *d;

  size = fuse_dirent_size(namelen_);

  rv = fuse_dirents_buf_resize(d_,size);
  if(rv)
    return NULL;

  d = (fuse_dirent_t*)&d_->buf[d_->data_len];

  d_->data_len += size;

  return d;
}

static
void*
fuse_dirents_direntplus_alloc(fuse_dirents_t *d_,
                              uint64_t        namelen_)
{
  int rv;
  uint64_t size;
  fuse_dirent_t *d;

  size = fuse_direntplus_size(namelen_);

  rv = fuse_dirents_buf_resize(d_,size);
  if(rv)
    return NULL;

  d = (fuse_dirent_t*)&d_->buf[d_->data_len];

  d_->data_len += size;

  return d;
}

static
void
fuse_dirents_fill_attr(fuse_attr_t       *attr_,
                       const struct stat *st_)
{
  attr_->ino       = st_->st_ino;
  attr_->size      = st_->st_size;
  attr_->blocks    = st_->st_blocks;
  attr_->atime     = st_->st_atime;
  attr_->mtime     = st_->st_mtime;
  attr_->ctime     = st_->st_ctime;
  attr_->atimensec = ST_ATIM_NSEC(st_);
  attr_->mtimensec = ST_MTIM_NSEC(st_);
  attr_->ctimensec = ST_CTIM_NSEC(st_);
  attr_->mode      = st_->st_mode;
  attr_->nlink     = st_->st_nlink;
  attr_->uid       = st_->st_uid;
  attr_->gid       = st_->st_gid;
  attr_->rdev      = st_->st_rdev;
  attr_->blksize   = st_->st_blksize;
}

fuse_dirent_t*
fuse_dirent_next(fuse_dirent_t *cur_)
{
  char *buf;

  buf  = (char*)cur_;
  buf += fuse_dirent_size(cur_->namelen);

  return (fuse_dirent_t*)buf;
}

fuse_direntplus_t*
fuse_direntplus_next(fuse_direntplus_t *cur_)
{
  char *buf;

  buf  = (char*)cur_;
  buf += fuse_direntplus_size(cur_->dirent.namelen);

  return (fuse_direntplus_t*)buf;
}

fuse_dirent_t*
fuse_dirent_find(fuse_dirents_t *d_,
                 const uint64_t  ino_)
{
  fuse_dirent_t *cur;
  fuse_dirent_t *end;

  if(d_->type != NORMAL)
    return NULL;

  cur = (fuse_dirent_t*)&d_->buf[0];
  end = (fuse_dirent_t*)&d_->buf[d_->data_len];
  while(cur < end)
    {
      if(cur->ino == ino_)
        return cur;

      cur = fuse_dirent_next(cur);
    }

  return NULL;
}

fuse_direntplus_t*
fuse_direntplus_find(fuse_dirents_t *d_,
                     const uint64_t  ino_)
{
  fuse_direntplus_t *cur;
  fuse_direntplus_t *end;

  if(d_->type != PLUS)
    return NULL;

  cur = (fuse_direntplus_t*)&d_->buf[0];
  end = (fuse_direntplus_t*)&d_->buf[d_->data_len];
  while(cur < end)
    {
      if(cur->dirent.ino == ino_)
        return cur;

      cur = fuse_direntplus_next(cur);
    }

  return NULL;
}

void*
fuse_dirents_find(fuse_dirents_t *d_,
                  const uint64_t  ino_)
{
  switch(d_->type)
    {
    default:
    case UNSET:
      return NULL;
    case NORMAL:
      return fuse_dirent_find(d_,ino_);
    case PLUS:
      return fuse_direntplus_find(d_,ino_);
    }
}

int
fuse_dirents_convert_plus2normal(fuse_dirents_t *d_)
{
  return -ENOSYS;
}

int
fuse_dirents_add(fuse_dirents_t      *d_,
                 const struct dirent *dirent_,
                 const uint64_t       namelen_)
{
  fuse_dirent_t *d;

  switch(d_->type)
    {
    case UNSET:
      d_->type = NORMAL;
      break;
    case NORMAL:
      break;
    case PLUS:
      return -EINVAL;
    }

  d = fuse_dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,d_->data_len);
  d->ino     = dirent_->d_ino;
  d->namelen = namelen_;
  d->type    = dirent_->d_type;
  memcpy(d->name,dirent_->d_name,namelen_);

  return 0;
}

int
fuse_dirents_add_plus(fuse_dirents_t      *d_,
                      const struct dirent *dirent_,
                      const uint64_t       namelen_,
                      const fuse_entry_t  *entry_,
                      const struct stat   *st_)
{
  fuse_direntplus_t *d;

  switch(d_->type)
    {
    case UNSET:
      d_->type = PLUS;
      break;
    case NORMAL:
      return -EINVAL;
    case PLUS:
      break;
    }

  d = fuse_dirents_direntplus_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->dirent.off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,d_->data_len);
  d->dirent.ino     = dirent_->d_ino;
  d->dirent.namelen = namelen_;
  d->dirent.type    = dirent_->d_type;
  memcpy(d->dirent.name,dirent_->d_name,namelen_);

  d->entry = *entry_;

  fuse_dirents_fill_attr(&d->attr,st_);

  return 0;
}

int
fuse_dirents_add_linux(fuse_dirents_t              *d_,
                       const struct linux_dirent64 *dirent_,
                       const uint64_t               namelen_)
{
  fuse_dirent_t *d;

  switch(d_->type)
    {
    case UNSET:
      d_->type = NORMAL;
      break;
    case NORMAL:
      break;
    case PLUS:
      return -EINVAL;
    }

  d = fuse_dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,d_->data_len);
  d->ino     = dirent_->ino;
  d->namelen = namelen_;
  d->type    = dirent_->type;
  memcpy(d->name,dirent_->name,namelen_);

  return 0;
}

int
fuse_dirents_add_linux_plus(fuse_dirents_t              *d_,
                            const struct linux_dirent64 *dirent_,
                            const uint64_t               namelen_,
                            const fuse_entry_t          *entry_,
                            const struct stat           *st_)
{
  fuse_direntplus_t *d;

  switch(d_->type)
    {
    case UNSET:
      d_->type = PLUS;
      break;
    case NORMAL:
      return -EINVAL;
    case PLUS:
      break;
    }

  d = fuse_dirents_direntplus_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->dirent.off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,d_->data_len);
  d->dirent.ino     = dirent_->ino;
  d->dirent.namelen = namelen_;
  d->dirent.type    = dirent_->type;
  memcpy(d->dirent.name,dirent_->name,namelen_);

  d->entry = *entry_;

  fuse_dirents_fill_attr(&d->attr,st_);

  return 0;
}

void
fuse_dirents_reset(fuse_dirents_t *d_)
{
  d_->data_len      = 0;
  d_->type          = UNSET;
  kv_size(d_->offs) = 1;
}

int
fuse_dirents_init(fuse_dirents_t *d_)
{
  void *buf;

  buf = calloc(DEFAULT_SIZE,1);
  if(buf == NULL)
    return -ENOMEM;

  d_->buf      = buf;
  d_->buf_len  = DEFAULT_SIZE;
  d_->data_len = 0;
  d_->type     = UNSET;

  kv_init(d_->offs);
  kv_resize(uint32_t,d_->offs,64);
  kv_push(uint32_t,d_->offs,0);

  return 0;
}

void
fuse_dirents_free(fuse_dirents_t *d_)
{
  d_->buf_len  = 0;
  d_->data_len = 0;
  d_->type     = UNSET;

  kv_destroy(d_->offs);

  free(d_->buf);
}

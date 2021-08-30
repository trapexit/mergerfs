/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "kvec.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define ROUND_UP(N,S) ((((N) + (S) - 1) / (S)) * (S))

typedef kvec_t(void*) slab_kvec_t;

typedef struct mem_stack_t mem_stack_t;
struct mem_stack_t
{
  mem_stack_t *next;
};

typedef struct fmp_t fmp_t;
struct fmp_t
{
  mem_stack_t *objs;
  slab_kvec_t  slabs;
  uint64_t     avail_objs;
  uint64_t     obj_size;
  uint64_t     page_size;
  uint64_t     slab_size;
};

static
inline
uint64_t
fmp_page_size()
{
  return sysconf(_SC_PAGESIZE);
}

static
inline
void
fmp_init(fmp_t          *fmp_,
         const uint64_t  obj_size_,
         const uint64_t  page_multiple_)
{
  kv_init(fmp_->slabs);
  fmp_->objs       = NULL;
  fmp_->avail_objs = 0;
  fmp_->obj_size   = ROUND_UP(obj_size_,sizeof(void*));
  fmp_->page_size  = fmp_page_size();
  fmp_->slab_size  = (fmp_->page_size * page_multiple_);
}

static
inline
uint64_t
fmp_slab_count(fmp_t *fmp_)
{
  return kv_size(fmp_->slabs);
}

static
inline
void*
fmp_slab_alloc_posix_memalign(fmp_t *fmp_)
{
  int rv;
  void *mem;
  const size_t alignment = fmp_->page_size;
  const size_t size      = fmp_->slab_size;

  rv = posix_memalign(&mem,alignment,size);
  if(rv != 0)
    return NULL;

  return NULL;
}

static
inline
void*
fmp_slab_alloc_mmap(fmp_t *fmp_)
{
  void         *mem;
  void         *address = NULL;
  const size_t  length  = fmp_->slab_size;
  const int     protect = PROT_READ|PROT_WRITE;
  const int     flags   = MAP_PRIVATE|MAP_ANONYMOUS;
  const int     filedes = -1;
  const off_t   offset  = 0;

  mem = mmap(address,length,protect,flags,filedes,offset);
  if(mem == MAP_FAILED)
    return NULL;

  return mem;
}

static
inline
void
fmp_slab_free_posix_memalign(fmp_t*  fmp_,
                             void   *mem_)
{
  (void)fmp_;
  free(mem_);
}

static
inline
void
fmp_slab_free_mmap(fmp_t*  fmp_,
                   void   *mem_)
{
  void   *addr   = mem_;
  size_t  length = fmp_->slab_size;

  (void)munmap(addr,length);
}

static
inline
int
fmp_slab_alloc(fmp_t *fmp_)
{
  char *i;
  void *mem;

  mem = fmp_slab_alloc_mmap(fmp_);
  if(mem == NULL)
    return -ENOMEM;

  kv_push(void*,fmp_->slabs,mem);

  i = ((char*)mem + fmp_->slab_size - fmp_->obj_size);
  while(i >= (char*)mem)
    {
      mem_stack_t *obj = (mem_stack_t*)i;

      obj->next  = fmp_->objs;
      fmp_->objs = obj;
      fmp_->avail_objs++;

      i -= fmp_->obj_size;
    }

  return 0;
}

static
inline
void*
fmp_alloc(fmp_t *fmp_)
{
  void *rv;

  if(fmp_->objs == NULL)
    fmp_slab_alloc(fmp_);
  if(fmp_->objs == NULL)
    return NULL;

  rv = fmp_->objs;

  fmp_->objs = fmp_->objs->next;
  fmp_->avail_objs--;

  return rv;
}

static
inline
void*
fmp_calloc(fmp_t *fmp_)
{
  void *obj;

  obj = fmp_alloc(fmp_);
  if(obj == NULL)
    return NULL;

  memset(obj,0,fmp_->obj_size);

  return obj;
}

static
inline
void
fmp_free(fmp_t *fmp_,
         void  *obj_)
{
  mem_stack_t *obj = (mem_stack_t*)obj_;

  obj->next  = fmp_->objs;
  fmp_->objs = obj;
  fmp_->avail_objs++;
}

static
inline
void
fmp_clear(fmp_t *fmp_)
{
  while(kv_size(fmp_->slabs))
    {
      void *slab = kv_pop(fmp_->slabs);

      fmp_slab_free_mmap(fmp_,slab);
    }

  fmp_->objs       = NULL;
  fmp_->avail_objs = 0;
}

static
inline
void
fmp_destroy(fmp_t *fmp_)
{
  fmp_clear(fmp_);
  kv_destroy(fmp_->slabs);
}

static
inline
uint64_t
fmp_avail_objs(fmp_t *fmp_)
{
  return fmp_->avail_objs;
}

static
inline
uint64_t
fmp_objs_in_slab(fmp_t *fmp_,
                 void  *slab_)
{
  char *slab;
  uint64_t objs_in_slab;

  objs_in_slab = 0;
  slab = (char*)slab_;
  for(mem_stack_t *stack = fmp_->objs; stack != NULL; stack = stack->next)
    {
      char *obj = (char*)stack;
      if((obj >= slab) && (obj < (slab + fmp_->slab_size)))
        objs_in_slab++;
    }

  return objs_in_slab;
}

static
inline
void
fmp_remove_objs_in_slab(fmp_t *fmp_,
                        void  *slab_)
{
  char *slab = (char*)slab_;
  mem_stack_t **p = &fmp_->objs;

  while((*p) != NULL)
    {
      char *obj = (char*)*p;

      if((obj >= slab) && (obj < (slab + fmp_->slab_size)))
        {
          *p = (*p)->next;
          fmp_->avail_objs--;
          continue;
        }

      p = &(*p)->next;
    }
}

static
inline
int
fmp_gc(fmp_t *fmp_)
{
  int i;
  int freed_slabs;
  uint64_t objs_per_slab;

  objs_per_slab = (fmp_->slab_size / fmp_->obj_size);

  i = 0;
  freed_slabs = 0;
  while(i < kv_size(fmp_->slabs))
    {
      char *slab;
      uint64_t objs_in_slab;

      slab = kv_A(fmp_->slabs,i);

      objs_in_slab = fmp_objs_in_slab(fmp_,slab);
      if(objs_in_slab != objs_per_slab)
        {
          i++;
          continue;
        }

      fmp_remove_objs_in_slab(fmp_,slab);

      kv_delete(fmp_->slabs,i);

      fmp_slab_free_mmap(fmp_,slab);
      freed_slabs++;
    }

  return freed_slabs;
}

static
inline
uint64_t
fmp_objs_per_slab(fmp_t *fmp_)
{
  return (fmp_->slab_size / fmp_->obj_size);
}

static
inline
double
fmp_slab_usage_ratio(fmp_t *fmp_)
{
  double rv;
  uint64_t objs_per_slab;

  objs_per_slab = fmp_objs_per_slab(fmp_);

  rv = ((double)fmp_->avail_objs / (double)objs_per_slab);

  return rv;
}

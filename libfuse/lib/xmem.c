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

#include "khash.h"

#include <pthread.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct meminfo_s meminfo_t;
struct meminfo_s
{
  const char *file;
  int         line;
  size_t      size;
};

#define kh_voidp_hash_func(KEY) ((((uint64_t)KEY)>>33)^((uint64_t)KEY)^(((uint64_t)KEY)<<11))
#define kh_voidp_hash_equal(A,B) ((A) == (B))
#define kh_meminfo_hash_func(KEY) ((((uint64_t)KEY.file)>>33)^((uint64_t)KEY.file)^(((uint64_t)KEY.file)<<11))
#define kh_meminfo_hash_equal(A,B) (((A).file == (B).file) && ((A).line == (B).line))

KHASH_INIT(mem,void*,meminfo_t,1,kh_voidp_hash_func,kh_voidp_hash_equal)
KHASH_INIT(memrev,meminfo_t,uint64_t,1,kh_meminfo_hash_func,kh_meminfo_hash_equal)


static int              g_ENABLED = 0;
static kh_mem_t        *g_HASH    = NULL;
static pthread_mutex_t  g_HASH_MUTEX;
static int              g_PRINT   = 0;

__attribute__((constructor))
static
void
xmem_init(void)
{
  pthread_mutex_init(&g_HASH_MUTEX,NULL);
  g_HASH = kh_init_mem();
}

__attribute__((destructor))
static
void
xmem_destroy(void)
{
  kh_destroy_mem(g_HASH);
}

void
xmem_enable(void)
{
  g_ENABLED = 1;
}

void
xmem_disable(void)
{
  g_ENABLED = 0;
}

void
xmem_print(FILE *file_)
{
  khint_t k;
  kh_memrev_t *rev;

  if(file_ == NULL)
    file_ = stderr;

  rev = kh_init_memrev();

  pthread_mutex_lock(&g_HASH_MUTEX);

  for(k = kh_begin(g_HASH); k != kh_end(g_HASH); ++k)
    {
      if(kh_exist(g_HASH,k))
        {
          int ret;
          khint_t kr;
          meminfo_t *mi;

          mi = &kh_value(g_HASH,k);

          kr = kh_put(memrev,rev,*mi,&ret);
          if(ret == 1)
            kh_value(rev,kr) = 0;
          kh_value(rev,kr) += mi->size;
        }
    }

  pthread_mutex_unlock(&g_HASH_MUTEX);

  for(k = kh_begin(rev); k != kh_end(rev); ++k)
    {
      if(!kh_exist(rev,k))
        continue;

      fprintf(file_,
              "%s:%d: %lu\n",
              kh_key(rev,k).file,
              kh_key(rev,k).line,
              kh_value(rev,k));
    }

  kh_destroy_memrev(rev);
}

void
xmem_print_to_tmp(void)
{
  FILE *file;
  char filepath[256];

  sprintf(filepath,"/tmp/xmem.%d.%ld",getpid(),time(NULL));
  file = fopen(filepath,"w+");

  xmem_print(file);

  fclose(file);
}

void*
xmem_malloc_(size_t      size_,
             const char *file_,
             const int   line_)
{
  void *rv;

  if(g_ENABLED == 0)
    return malloc(size_);

  pthread_mutex_lock(&g_HASH_MUTEX);

  rv = malloc(size_);
  if(rv != NULL)
    {
      int ret;
      khint_t k;
      meminfo_t *meminfo;

      k = kh_put(mem,g_HASH,rv,&ret);

      meminfo = &kh_value(g_HASH,k);

      meminfo->file = file_;
      meminfo->line = line_;
      meminfo->size = size_;
    }

  pthread_mutex_unlock(&g_HASH_MUTEX);

  if(g_PRINT)
    fprintf(stderr,"malloc: %lu bytes @ %p - %s:%d\n",size_,rv,file_,line_);

  return rv;
}

void*
xmem_realloc_(void       *ptr_,
              size_t      size_,
              const char *file_,
              const int   line_)
{
  void *rv;

  if(g_ENABLED == 0)
    return realloc(ptr_,size_);

  if(ptr_ == NULL)
    return xmem_malloc_(size_,file_,line_);

  pthread_mutex_lock(&g_HASH_MUTEX);

  rv = realloc(ptr_,size_);
  if(rv != NULL)
    {
      int ret;
      khint_t k;
      meminfo_t *meminfo;

      k = kh_get(mem,g_HASH,ptr_);
      if(k != kh_end(g_HASH))
        {
          if(rv != ptr_)
            {
              kh_del(mem,g_HASH,k);
              k = kh_put(mem,g_HASH,rv,&ret);
            }

          meminfo = &kh_value(g_HASH,k);

          meminfo->file = file_;
          meminfo->line = line_;
          meminfo->size = size_;
        }
    }

  pthread_mutex_unlock(&g_HASH_MUTEX);

  if(g_PRINT)
    fprintf(stderr,"realloc: %lu bytes @ %p - %s:%d\n",size_,rv,file_,line_);

  return rv;
}

void*
xmem_calloc_(size_t      nmemb_,
             size_t      size_,
             const char *file_,
             const int   line_)
{
  void *rv;

  if(g_ENABLED == 0)
    return calloc(nmemb_,size_);

  pthread_mutex_lock(&g_HASH_MUTEX);

  rv = calloc(nmemb_,size_);
  if(rv != NULL)
    {
      int ret;
      khint_t k;
      meminfo_t *meminfo;

      k = kh_put(mem,g_HASH,rv,&ret);

      meminfo = &kh_value(g_HASH,k);

      meminfo->file = file_;
      meminfo->line = line_;
      meminfo->size = (nmemb_ * size_);
    }

  pthread_mutex_unlock(&g_HASH_MUTEX);

  if(g_PRINT)
    fprintf(stderr,"calloc: %lu bytes @ %p - %s:%d\n",nmemb_*size_,rv,file_,line_);

  return rv;
}

char*
xmem_strdup_(const char *s_,
             const char *file_,
             const int   line_)
{
  char *rv;
  size_t size;

  if(g_ENABLED == 0)
    return strdup(s_);

  pthread_mutex_lock(&g_HASH_MUTEX);

  size = 0;
  rv   = strdup(s_);
  if(rv != NULL)
    {
      int ret;
      khint_t k;
      meminfo_t *meminfo;

      k = kh_put(mem,g_HASH,rv,&ret);

      meminfo = &kh_value(g_HASH,k);

      size = strlen(s_);
      meminfo->file = file_;
      meminfo->line = line_;
      meminfo->size = size;
    }

  pthread_mutex_unlock(&g_HASH_MUTEX);

  if(g_PRINT)
    fprintf(stderr,"strdup: %lu bytes @ %p - %s:%d\n",size,rv,file_,line_);

  return rv;
}

void
xmem_free_(void       *ptr_,
           const char *file_,
           const int   line_)
{
  khint_t key;

  if(g_ENABLED == 0)
    return free(ptr_);

  pthread_mutex_lock(&g_HASH_MUTEX);

  free(ptr_);
  key = kh_get(mem,g_HASH,ptr_);
  if(key != kh_end(g_HASH))
    kh_del(mem,g_HASH,key);

  pthread_mutex_unlock(&g_HASH_MUTEX);

  if(g_PRINT)
    fprintf(stderr,"free: %p  - %s:%d\n",ptr_,file_,line_);
}

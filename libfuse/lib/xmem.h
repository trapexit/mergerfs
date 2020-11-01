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

#include <stdio.h>
#include <stdlib.h>

extern void  xmem_enable(void);
extern void  xmem_disable(void);

extern void *xmem_malloc_(size_t size, const char *file, const int line);
extern void *xmem_realloc_(void *ptr, size_t size, const char *file, const int line);
extern void *xmem_calloc_(size_t nmemb, size_t size, const char *file, const int line);
extern void  xmem_free_(void *ptr, const char *file, const int line);
extern char *xmem_strdup_(const char *s, const char *file, const int line);

extern void  xmem_print(FILE *file);

#define xmem_malloc(SIZE) (xmem_malloc_((SIZE),__FILE__,__LINE__))
#define xmem_realloc(PTR,SIZE) (xmem_realloc_((PTR),(SIZE),__FILE__,__LINE__))
#define xmem_calloc(NMEMB,SIZE) (xmem_calloc_((NMEMB),(SIZE),__FILE__,__LINE__))
#define xmem_free(PTR) (xmem_free_((PTR),__FILE__,__LINE__))
#define xmem_strdup(STR) (xmem_strdup_((STR),__FILE__,__LINE__))

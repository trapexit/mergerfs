/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "extern_c.h"

EXTERN_C_BEGIN

#include "kvec.h"
#include "fuse_dirent.h"
#include "fuse_direntplus.h"
#include "fuse_entry.h"
#include "linux_dirent64.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum fuse_dirents_type_e
{
  UNSET = 0,
  NORMAL,
  PLUS
};
typedef enum fuse_dirents_type_e fuse_dirents_type_t;

typedef struct fuse_dirents_t fuse_dirents_t;
struct fuse_dirents_t
{
  kvec_t(char)        data;
  kvec_t(uint32_t)    offs;
  fuse_dirents_type_t type;
};

int  fuse_dirents_init(fuse_dirents_t *d);
void fuse_dirents_free(fuse_dirents_t *d);
void fuse_dirents_reset(fuse_dirents_t *d);

int  fuse_dirents_add(fuse_dirents_t      *d,
                      const struct dirent *de,
                      const uint64_t       namelen);
int  fuse_dirents_add_plus(fuse_dirents_t      *d,
                           const struct dirent *de,
                           const uint64_t       namelen,
                           const fuse_entry_t  *entry,
                           const struct stat   *st);
int  fuse_dirents_add_linux(fuse_dirents_t         *d,
                            const linux_dirent64_t *de,
                            const uint64_t          namelen);
int  fuse_dirents_add_linux_plus(fuse_dirents_t         *d,
                                 const linux_dirent64_t *de,
                                 const uint64_t          namelen,
                                 const fuse_entry_t     *entry,
                                 const struct stat      *st);

void *fuse_dirents_find(fuse_dirents_t *d,
                        const uint64_t  ino);

int fuse_dirents_convert_plus2normal(fuse_dirents_t *d);

EXTERN_C_END

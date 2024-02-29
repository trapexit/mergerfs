/*
  ISC License

  Copyright (c) 2024, Antonio SJ Musumeci <trapexit@spawn.link>

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

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

typedef char IOCTL_BUF[4096];
#define IOCTL_APP_TYPE  0xDF
#define IOCTL_FILE_INFO _IOWR(IOCTL_APP_TYPE,0,IOCTL_BUF)

#define LOAD_FUNC(func)                                         \
  do                                                            \
    {                                                           \
      if(!_libc_##func)                                         \
        _libc_##func = (func##_func_t)dlsym(RTLD_NEXT,#func);   \
      assert(_libc_##func != NULL);                             \
    }                                                           \
  while(0)

typedef int (*open_func_t)(const char*, int, ...);
typedef int (*open64_func_t)(const char*, int, ...);
typedef int (*openat_func_t)(int, const char*, int, ...);
typedef int (*openat64_func_t)(int, const char*, int, ...);
typedef int (*creat_func_t)(const char*, mode_t);
typedef int (*creat64_func_t)(const char*, mode_t);
typedef FILE* (*fopen_func_t)(const char*, const char*);
typedef FILE* (*fopen64_func_t)(const char*, const char*);

static open_func_t     _libc_open     = NULL;
static open64_func_t   _libc_open64   = NULL;
static openat_func_t   _libc_openat   = NULL;
static openat64_func_t _libc_openat64 = NULL;
static fopen_func_t    _libc_fopen    = NULL;
static fopen64_func_t  _libc_fopen64  = NULL;
static creat_func_t    _libc_creat    = NULL;
static creat64_func_t  _libc_creat64  = NULL;

static
int
get_underlying_filepath(int   fd_,
                        char *filepath_)
{
  int rv;

  strcpy(filepath_,"fullpath");
  rv = ioctl(fd_,IOCTL_FILE_INFO,filepath_);
  if(rv == -1)
    return -1;

  return rv;
}

static
void
strip_exec(const char *orig_mode_,
           char       *new_mode_)
{
  size_t i;
  size_t j;

  for(i = j = 0; orig_mode_[i]; i++)
    {
      if(orig_mode_[i] == 'x')
        continue;
      new_mode_[j++] = orig_mode_[i];
    }

  new_mode_[j] = '\0';
}

int
open(const char *pathname_,
     int         flags_,
     ...)
{
  int rv;
  int fd;
  mode_t mode;
  struct stat st;

  LOAD_FUNC(open);

  mode = 0;
  if(flags_ & (O_CREAT|O_TMPFILE))
    {
      va_list args;
      va_start(args,flags_);
      mode = va_arg(args,mode_t);
      va_end(args);
    }

  fd = _libc_open(pathname_,flags_,mode);
  if(fd == -1)
    return -1;

  if(flags_ & (O_DIRECTORY|O_PATH|O_TMPFILE))
    return fd;
  rv = fstat(fd,&st);
  if(rv == -1)
    return fd;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return fd;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  flags_ &= ~(O_EXCL|O_CREAT);
  rv = _libc_open(real_pathname,flags_,mode);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

int
open64(const char *pathname_,
       int         flags_,
       ...)
{
  int rv;
  int fd;
  mode_t mode;
  struct stat st;

  LOAD_FUNC(open64);

  mode = 0;
  if(flags_ & (O_CREAT|O_TMPFILE))
    {
      va_list args;
      va_start(args,flags_);
      mode = va_arg(args,mode_t);
      va_end(args);
    }

  fd = _libc_open64(pathname_,flags_,mode);
  if(fd == -1)
    return -1;

  if(flags_ & (O_DIRECTORY|O_PATH|O_TMPFILE))
    return fd;
  rv = fstat(fd,&st);
  if(rv == -1)
    return fd;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return fd;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  flags_ &= ~(O_EXCL|O_CREAT);
  rv = _libc_open64(real_pathname,flags_,mode);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

int
openat(int         dirfd_,
       const char *pathname_,
       int         flags_,
       ...)
{
  int rv;
  int fd;
  mode_t mode;
  struct stat st;

  LOAD_FUNC(openat);

  mode = 0;
  if(flags_ & (O_CREAT|O_TMPFILE))
    {
      va_list args;
      va_start(args,flags_);
      mode = va_arg(args,mode_t);
      va_end(args);
    }

  fd = _libc_openat(dirfd_,pathname_,flags_,mode);
  if(fd == -1)
    return -1;

  if(flags_ & (O_DIRECTORY|O_PATH|O_TMPFILE))
    return fd;
  rv = fstat(fd,&st);
  if(rv == -1)
    return fd;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return fd;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  flags_ &= ~(O_EXCL|O_CREAT);
  rv = _libc_openat(dirfd_,real_pathname,flags_,mode);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

int
openat64(int         dirfd_,
         const char *pathname_,
         int         flags_,
         ...)
{
  int rv;
  int fd;
  mode_t mode;
  struct stat st;

  LOAD_FUNC(openat64);

  mode = 0;
  if(flags_ & (O_CREAT|O_TMPFILE))
    {
      va_list args;
      va_start(args,flags_);
      mode = va_arg(args,mode_t);
      va_end(args);
    }

  fd = _libc_openat64(dirfd_,pathname_,flags_,mode);
  if(fd == -1)
    return -1;

  if(flags_ & (O_DIRECTORY|O_PATH|O_TMPFILE))
    return fd;
  rv = fstat(fd,&st);
  if(rv == -1)
    return fd;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return fd;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  flags_ &= ~(O_EXCL|O_CREAT);
  rv = _libc_openat64(dirfd_,real_pathname,flags_,mode);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

FILE*
fopen(const char *pathname_,
      const char *mode_)
{
  int fd;
  int rv;
  FILE *f;
  FILE *f2;
  struct stat st;

  LOAD_FUNC(fopen);

  f = _libc_fopen(pathname_,mode_);
  if(f == NULL)
    return NULL;

  fd = fileno(f);
  if(fd == -1)
    return f;

  rv = fstat(fd,&st);
  if(rv == -1)
    return f;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return f;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return f;

  char new_mode[64];
  strip_exec(mode_,new_mode);
  f2 = _libc_fopen(real_pathname,new_mode);
  if(f2 == NULL)
    return f;

  fclose(f);

  return f2;
}

FILE*
fopen64(const char *pathname_,
        const char *mode_)
{
  int fd;
  int rv;
  FILE *f;
  FILE *f2;
  struct stat st;

  LOAD_FUNC(fopen64);

  f = _libc_fopen64(pathname_,mode_);
  if(f == NULL)
    return NULL;

  fd = fileno(f);
  if(fd == -1)
    return f;

  rv = fstat(fd,&st);
  if(rv == -1)
    return f;
  if((st.st_mode & S_IFMT) != S_IFREG)
    return f;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return f;

  char new_mode[64];
  strip_exec(mode_,new_mode);
  f2 = _libc_fopen64(real_pathname,new_mode);
  if(f2 == NULL)
    return f;

  fclose(f);

  return f2;
}

int
creat(const char *pathname_,
      mode_t      mode_)
{
  int fd;
  int rv;

  LOAD_FUNC(creat);

  fd = _libc_creat(pathname_,mode_);
  if(fd == -1)
    return -1;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  rv = _libc_creat(real_pathname,mode_);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

int
creat64(const char *pathname_,
        mode_t      mode_)
{
  int fd;
  int rv;

  LOAD_FUNC(creat64);

  fd = _libc_creat64(pathname_,mode_);
  if(fd == -1)
    return -1;

  IOCTL_BUF real_pathname;
  rv = get_underlying_filepath(fd,real_pathname);
  if(rv == -1)
    return fd;

  rv = _libc_creat64(real_pathname,mode_);
  if(rv == -1)
    return fd;

  close(fd);

  return rv;
}

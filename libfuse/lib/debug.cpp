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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "syslog.hpp"

#include "fuse_kernel.h"

#include "fmt/core.h"

#include <string>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static FILE *g_OUTPUT = NULL;

#define PARAM(inarg) (((char *)(inarg)) + sizeof(*(inarg)))

static
int
debug_set_output_null()
{
  g_OUTPUT = stderr;
  setvbuf(g_OUTPUT,NULL,_IOLBF,0);

  return 0;
}

static
int
debug_set_output_filepath(const char *filepath_)
{
  FILE *tmp;

  tmp = fopen(filepath_,"a");
  if(tmp == NULL)
    return -errno;

  g_OUTPUT = tmp;
  setvbuf(g_OUTPUT,NULL,_IOLBF,0);

  return 0;
}

int
debug_set_output(const char *filepath_)
{
  if(filepath_ == NULL)
    return debug_set_output_null();

  return debug_set_output_filepath(filepath_);
}

static
__attribute__((constructor))
void
debug_constructor(void)
{
  debug_set_output(NULL);
}

static
const
char*
open_accmode_to_str(const int flags_)
{
  switch(flags_ & O_ACCMODE)
    {
    case O_RDWR:
      return "O_RDWR";
    case O_RDONLY:
      return "O_RDONLY";
    case O_WRONLY:
      return "O_WRONLY";
    }

  return "";
}

#define FUSE_WRITE_FLAG_CASE(X) case FUSE_WRITE_##X: return #X

static
const
char*
fuse_write_flag_to_str(const uint32_t offset_)
{
  switch(1 << offset_)
    {
      FUSE_WRITE_FLAG_CASE(CACHE);
      FUSE_WRITE_FLAG_CASE(LOCKOWNER);
      FUSE_WRITE_FLAG_CASE(KILL_PRIV);
    }

  return NULL;
}

#undef FUSE_WRITE_FLAG_CASE

#define OPEN_FLAG_CASE(X) case X: return #X

static
const
char*
open_flag_to_str(const uint64_t offset_)
{
  switch(1 << offset_)
    {
      OPEN_FLAG_CASE(O_APPEND);
      OPEN_FLAG_CASE(O_ASYNC);
      OPEN_FLAG_CASE(O_CLOEXEC);
      OPEN_FLAG_CASE(O_CREAT);
      OPEN_FLAG_CASE(O_DIRECT);
      OPEN_FLAG_CASE(O_DIRECTORY);
#ifdef O_DSYNC
      OPEN_FLAG_CASE(O_DSYNC);
#endif
      OPEN_FLAG_CASE(O_EXCL);
#ifdef O_LARGEFILE
      OPEN_FLAG_CASE(O_LARGEFILE);
#endif
#ifdef O_NOATIME
      OPEN_FLAG_CASE(O_NOATIME);
#endif
      OPEN_FLAG_CASE(O_NOCTTY);
      OPEN_FLAG_CASE(O_NOFOLLOW);
      OPEN_FLAG_CASE(O_NONBLOCK);
#ifdef O_PATH
      OPEN_FLAG_CASE(O_PATH);
#endif
      OPEN_FLAG_CASE(O_SYNC);
#ifdef O_TMPFILE
      OPEN_FLAG_CASE(O_TMPFILE);
#endif
      OPEN_FLAG_CASE(O_TRUNC);
    }

  return NULL;
}

#undef OPEN_FLAG_CASE

#define FUSE_INIT_FLAG_CASE(X) case FUSE_##X: return #X

static
const
char*
fuse_flag_to_str(const uint64_t offset_)
{
  switch(1ULL << offset_)
    {
      FUSE_INIT_FLAG_CASE(ASYNC_READ);
      FUSE_INIT_FLAG_CASE(POSIX_LOCKS);
      FUSE_INIT_FLAG_CASE(FILE_OPS);
      FUSE_INIT_FLAG_CASE(ATOMIC_O_TRUNC);
      FUSE_INIT_FLAG_CASE(EXPORT_SUPPORT);
      FUSE_INIT_FLAG_CASE(BIG_WRITES);
      FUSE_INIT_FLAG_CASE(DONT_MASK);
      FUSE_INIT_FLAG_CASE(SPLICE_WRITE);
      FUSE_INIT_FLAG_CASE(SPLICE_MOVE);
      FUSE_INIT_FLAG_CASE(SPLICE_READ);
      FUSE_INIT_FLAG_CASE(FLOCK_LOCKS);
      FUSE_INIT_FLAG_CASE(HAS_IOCTL_DIR);
      FUSE_INIT_FLAG_CASE(AUTO_INVAL_DATA);
      FUSE_INIT_FLAG_CASE(DO_READDIRPLUS);
      FUSE_INIT_FLAG_CASE(READDIRPLUS_AUTO);
      FUSE_INIT_FLAG_CASE(ASYNC_DIO);
      FUSE_INIT_FLAG_CASE(WRITEBACK_CACHE);
      FUSE_INIT_FLAG_CASE(NO_OPEN_SUPPORT);
      FUSE_INIT_FLAG_CASE(PARALLEL_DIROPS);
      FUSE_INIT_FLAG_CASE(HANDLE_KILLPRIV);
      FUSE_INIT_FLAG_CASE(POSIX_ACL);
      FUSE_INIT_FLAG_CASE(ABORT_ERROR);
      FUSE_INIT_FLAG_CASE(MAX_PAGES);
      FUSE_INIT_FLAG_CASE(CACHE_SYMLINKS);
      FUSE_INIT_FLAG_CASE(NO_OPENDIR_SUPPORT);
      FUSE_INIT_FLAG_CASE(EXPLICIT_INVAL_DATA);
      FUSE_INIT_FLAG_CASE(MAP_ALIGNMENT);
      FUSE_INIT_FLAG_CASE(SUBMOUNTS);
      FUSE_INIT_FLAG_CASE(HANDLE_KILLPRIV_V2);
      FUSE_INIT_FLAG_CASE(SETXATTR_EXT);
      FUSE_INIT_FLAG_CASE(INIT_EXT);
      FUSE_INIT_FLAG_CASE(INIT_RESERVED);
      FUSE_INIT_FLAG_CASE(SECURITY_CTX);
      FUSE_INIT_FLAG_CASE(HAS_INODE_DAX);
      FUSE_INIT_FLAG_CASE(CREATE_SUPP_GROUP);
      FUSE_INIT_FLAG_CASE(HAS_EXPIRE_ONLY);
      FUSE_INIT_FLAG_CASE(DIRECT_IO_ALLOW_MMAP);
      FUSE_INIT_FLAG_CASE(PASSTHROUGH);
      FUSE_INIT_FLAG_CASE(NO_EXPORT_SUPPORT);
      FUSE_INIT_FLAG_CASE(HAS_RESEND);
      FUSE_INIT_FLAG_CASE(ALLOW_IDMAP);
      FUSE_INIT_FLAG_CASE(OVER_IO_URING);
    }

  return NULL;
}

std::string
fuse_debug_init_flag_name(const uint64_t flag_)
{
  switch(flag_)
    {
      FUSE_INIT_FLAG_CASE(ASYNC_READ);
      FUSE_INIT_FLAG_CASE(POSIX_LOCKS);
      FUSE_INIT_FLAG_CASE(FILE_OPS);
      FUSE_INIT_FLAG_CASE(ATOMIC_O_TRUNC);
      FUSE_INIT_FLAG_CASE(EXPORT_SUPPORT);
      FUSE_INIT_FLAG_CASE(BIG_WRITES);
      FUSE_INIT_FLAG_CASE(DONT_MASK);
      FUSE_INIT_FLAG_CASE(SPLICE_WRITE);
      FUSE_INIT_FLAG_CASE(SPLICE_MOVE);
      FUSE_INIT_FLAG_CASE(SPLICE_READ);
      FUSE_INIT_FLAG_CASE(FLOCK_LOCKS);
      FUSE_INIT_FLAG_CASE(HAS_IOCTL_DIR);
      FUSE_INIT_FLAG_CASE(AUTO_INVAL_DATA);
      FUSE_INIT_FLAG_CASE(DO_READDIRPLUS);
      FUSE_INIT_FLAG_CASE(READDIRPLUS_AUTO);
      FUSE_INIT_FLAG_CASE(ASYNC_DIO);
      FUSE_INIT_FLAG_CASE(WRITEBACK_CACHE);
      FUSE_INIT_FLAG_CASE(NO_OPEN_SUPPORT);
      FUSE_INIT_FLAG_CASE(PARALLEL_DIROPS);
      FUSE_INIT_FLAG_CASE(HANDLE_KILLPRIV);
      FUSE_INIT_FLAG_CASE(POSIX_ACL);
      FUSE_INIT_FLAG_CASE(ABORT_ERROR);
      FUSE_INIT_FLAG_CASE(MAX_PAGES);
      FUSE_INIT_FLAG_CASE(CACHE_SYMLINKS);
      FUSE_INIT_FLAG_CASE(NO_OPENDIR_SUPPORT);
      FUSE_INIT_FLAG_CASE(EXPLICIT_INVAL_DATA);
      FUSE_INIT_FLAG_CASE(MAP_ALIGNMENT);
      FUSE_INIT_FLAG_CASE(SUBMOUNTS);
      FUSE_INIT_FLAG_CASE(HANDLE_KILLPRIV_V2);
      FUSE_INIT_FLAG_CASE(SETXATTR_EXT);
      FUSE_INIT_FLAG_CASE(INIT_EXT);
      FUSE_INIT_FLAG_CASE(INIT_RESERVED);
      FUSE_INIT_FLAG_CASE(SECURITY_CTX);
      FUSE_INIT_FLAG_CASE(HAS_INODE_DAX);
      FUSE_INIT_FLAG_CASE(CREATE_SUPP_GROUP);
      FUSE_INIT_FLAG_CASE(HAS_EXPIRE_ONLY);
      FUSE_INIT_FLAG_CASE(DIRECT_IO_ALLOW_MMAP);
      FUSE_INIT_FLAG_CASE(PASSTHROUGH);
      FUSE_INIT_FLAG_CASE(NO_EXPORT_SUPPORT);
      FUSE_INIT_FLAG_CASE(HAS_RESEND);
      FUSE_INIT_FLAG_CASE(ALLOW_IDMAP);
      FUSE_INIT_FLAG_CASE(OVER_IO_URING);
    }

  return "unknown";
}

#undef FUSE_INIT_FLAG_CASE

static
void
debug_open_flags(const uint32_t flags_)
{
  fmt::print("{}, ",open_accmode_to_str(flags_));
  for(size_t i = 0; i < (sizeof(flags_) * 8); i++)
    {
      const char *str;

      if(!(flags_ & (1 << i)))
        continue;

      str = open_flag_to_str(i);
      if(str == NULL)
        continue;

      fmt::print("{}, ",str);
    }
}

#define FOPEN_FLAG_CASE(X) case FOPEN_##X: return #X

static
const
char*
fuse_fopen_flag_to_str(const uint32_t offset_)
{
  switch(1 << offset_)
    {
      FOPEN_FLAG_CASE(DIRECT_IO);
      FOPEN_FLAG_CASE(KEEP_CACHE);
      FOPEN_FLAG_CASE(NONSEEKABLE);
      FOPEN_FLAG_CASE(CACHE_DIR);
      FOPEN_FLAG_CASE(STREAM);
    }

  return NULL;
}

#undef FOPEN_FLAG_CASE

void
debug_fuse_open_out(const struct fuse_open_out *arg_)
{
  fmt::print(stderr,
             "fuse_open_out:"
             " fh=0x{:#08x};"
             " open_flags=0x{:#04x} (",
             arg_->fh,
             arg_->open_flags);
  for(size_t i = 0; i < (sizeof(arg_->open_flags) * 8); i++)
    {
      const char *str;

      if(!(arg_->open_flags & (1 << i)))
        continue;

      str = fuse_fopen_flag_to_str(i);
      if(str == NULL)
        continue;

      fmt::print(stderr,"{},",str);
    }
  fmt::print(stderr,");\n");
}

static
void
debug_fuse_lookup(const void *arg_)
{
  const char *name = (const char*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_lookup:"
             " name={};"
             "\n"
             ,
             name);
}

static
void
debug_fuse_getattr_in(const void *arg_)
{
  const struct fuse_getattr_in *arg = (const fuse_getattr_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_getattr_in:"
             " getattr_flags=0x{:#08x};"
             " fh=0x{:08x};\n",
             arg->getattr_flags,
             arg->fh);
}

static
void
debug_fuse_setattr_in(const void *arg_)
{
  const struct fuse_setattr_in *arg = (const fuse_setattr_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_setattr_in:"
             " valid={};"
             " fh=0x{:#08x};"
             " size={};"
             " lock_owner={};"
             " atime={};"
             " atimensec={};"
             " mtime={};"
             " mtimensec={};"
             " ctime={};"
             " ctimensec={};"
             " mode={:o};"
             " uid={};"
             " gid={};"
             "\n"
             ,
             arg->valid,
             arg->fh,
             arg->size,
             arg->lock_owner,
             arg->atime,
             arg->atimensec,
             arg->mtime,
             arg->mtimensec,
             arg->ctime,
             arg->ctimensec,
             arg->mode,
             arg->uid,
             arg->gid);
}

static
void
debug_fuse_access_in(const void *arg_)
{
  const struct fuse_access_in *arg = (const fuse_access_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_access_in:"
             " mask=0x{:#08x};"
             "\n"
             ,
             arg->mask);
}

static
void
debug_fuse_mknod_in(const void *arg_)
{
  const struct fuse_mknod_in *arg = (const fuse_mknod_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_mknod_in:"
             " mode={:o};"
             " rdev=0x{#:08x};"
             " umask={:o};"
             "\n"
             ,
             arg->mode,
             arg->rdev,
             arg->umask);
}

static
void
debug_fuse_mkdir_in(const void *arg_)
{
  const struct fuse_mkdir_in *arg = (const fuse_mkdir_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_mkdir_in:"
             " mode={:o};"
             " umask={:o};"
             " name=%s;"
             "\n"
             ,
             arg->mode,
             arg->umask,
             PARAM(arg));
}

static
void
debug_fuse_unlink(const void *arg_)
{
  const char *name = (const char*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_unlink:"
             " name=%s;"
             "\n"
             ,
             name);
}

static
void
debug_fuse_rmdir(const void *arg_)
{
  const char *name = (const char*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_mkdir:"
             " name=%s;"
             "\n"
             ,
             name);
}

static
void
debug_fuse_symlink(const void *arg_)
{
  const char *name;
  const char *linkname;

  name     = (const char*)arg_;
  linkname = (name + (strlen(name) + 1));

  fmt::print(g_OUTPUT,
             "fuse_mkdir:"
             " linkname=%s;"
             " name=%s;"
             "\n"
             ,
             linkname,
             name);
}

static
void
debug_fuse_rename_in(const void *arg_)
{
  const char *oldname;
  const char *newname;
  const struct fuse_rename_in *arg = (const fuse_rename_in*)arg_;

  oldname = PARAM(arg);
  newname = (oldname + strlen(oldname) + 1);

  fmt::print(g_OUTPUT,
             "fuse_rename_in:"
             " oldname=%s;"
             " newdir={};"
             " newname=%s;"
             "\n"
             ,
             oldname,
             arg->newdir,
             newname);
}

static
void
debug_fuse_link_in(const void *arg_)
{
  const char *name;
  const struct fuse_link_in *arg = (const fuse_link_in*)arg_;

  name = PARAM(arg);

  fmt::print(g_OUTPUT,
             "fuse_link_in:"
             " oldnodeid={};"
             " name=%s;"
             "\n"
             ,
             arg->oldnodeid,
             name);
}

static
void
debug_fuse_create_in(const void *arg_)
{
  const char *name;
  const struct fuse_create_in *arg = (const fuse_create_in*)arg_;

  name = PARAM(arg);

  fmt::print(g_OUTPUT,
             "fuse_create_in:"
             " mode={:o};"
             " umask={:o};"
             " name=%s;"
             " flags=0x%X (",
             arg->mode,
             arg->umask,
             name,
             arg->flags);
  debug_open_flags(arg->flags);
  fmt::print(g_OUTPUT,");\n");
}

static
void
debug_fuse_open_in(const void *arg_)
{
  const struct fuse_open_in *arg = (const fuse_open_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_open_in:"
             " flags=0x%08X (",
             arg->flags);
  debug_open_flags(arg->flags);
  fmt::print(g_OUTPUT,");\n");
}

static
void
debug_fuse_read_in(const void *arg_)
{
  const struct fuse_read_in *arg = (const fuse_read_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_read_in:"
             " fh=0x{:#08x};"
             " offset={};"
             " size={};"
             " read_flags=%X;"
             " lock_owner=0x{:#08x};"
             " flags=0x%X ("
             ,
             arg->fh,
             arg->offset,
             arg->size,
             arg->read_flags,
             arg->lock_owner,
             arg->flags);
  debug_open_flags(arg->flags);
  fmt::print(g_OUTPUT,");\n");
}

static
void
debug_fuse_write_in(const void *arg_)
{
  const struct fuse_write_in *arg = (const fuse_write_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_write_in:"
             " fh=0x{:#08x};"
             " offset={};"
             " size={};"
             " lock_owner=0x{:#08x};"
             " flags=0x%X ("
             ,
             arg->fh,
             arg->offset,
             arg->size,
             arg->lock_owner,
             arg->flags);
  debug_open_flags(arg->flags);
  fmt::print(g_OUTPUT,
             "); write_flags=0x%X (",
             arg->write_flags);
  for(size_t i = 0; i < (sizeof(arg->write_flags) * 8); i++)
    {
      const char *str;

      if(!(arg->write_flags & (1 << i)))
        continue;

      str = fuse_write_flag_to_str(i);
      if(str == NULL)
        continue;

      fmt::print(g_OUTPUT,"%s,",str);
    }
  fmt::print(g_OUTPUT,");\n");
}

static
void
debug_fuse_flush_in(const void *arg_)
{
  const struct fuse_flush_in *arg = (const fuse_flush_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_flush_in:"
             " fh=0x{:#08x};"
             " lock_owner=0x{:#08x};"
             "\n"
             ,
             arg->fh,
             arg->lock_owner);
}

static
void
debug_fuse_release_in(const void *arg_)
{
  const struct fuse_release_in *arg = (const fuse_release_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_release_in:"
             " fh=0x{:#08x};"
             " release_flags=0x%X;"
             " lock_owner=0x{:#08x};"
             " flags=0x%X ("
             ,
             arg->fh,
             arg->release_flags,
             arg->lock_owner,
             arg->flags);
  debug_open_flags(arg->flags);
  fmt::print(g_OUTPUT,");\n");
}

static
void
debug_fuse_fsync_in(const void *arg_)
{
  const struct fuse_fsync_in *arg = (const fuse_fsync_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_fsync_in:"
             " fh=0x{:#08x};"
             " fsync_flags=0x%X;"
             "\n"
             ,
             arg->fh,
             arg->fsync_flags);
}

static
void
debug_fuse_setxattr_in(const void *arg_)
{
  const char *name;
  const char *value;
  const struct fuse_setxattr_in *arg = (const fuse_setxattr_in*)arg_;

  name  = PARAM(arg);
  value = (name + strlen(name) + 1);

  fmt::print(g_OUTPUT,
             "fuse_setxattr_in:"
             " size={};"
             " flags=0x%X;"
             " name=%s;"
             " value=%s;"
             "\n"
             ,
             arg->size,
             arg->flags,
             name,
             value);
}

static
void
debug_fuse_getxattr_in(const void *arg_)
{
  const char *name;
  const struct fuse_getxattr_in *arg = (const fuse_getxattr_in*)arg_;

  name = PARAM(arg);

  fmt::print(g_OUTPUT,
             "fuse_getxattr_in:"
             " size={};"
             " name=%s;"
             "\n"
             ,
             arg->size,
             name);
}

static
void
debug_fuse_listxattr(const void *arg_)
{
  const struct fuse_getxattr_in *arg = (const fuse_getxattr_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_listxattr:"
             " size={};"
             "\n"
             ,
             arg->size);
}

static
void
debug_fuse_removexattr(const void *arg_)
{
  const char *name = (const char*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_removexattr:"
             " name=%s;"
             "\n"
             ,
             name);
}

static
void
debug_fuse_fallocate_in(const void *arg_)
{
  const struct fuse_fallocate_in *arg = (const fuse_fallocate_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_fallocate_in:"
             " fh=0x{:#08x};"
             " offset={};"
             " length={};"
             " mode={:o};"
             "\n"
             ,
             arg->fh,
             arg->offset,
             arg->length,
             arg->mode);
}

void
debug_fuse_init_in(const struct fuse_init_in *arg_)
{
  uint64_t flags;

  flags = (((uint64_t)arg_->flags) | ((uint64_t)arg_->flags2) << 32);
  fmt::print(g_OUTPUT,
             "FUSE_INIT_IN: "
             " major={};"
             " minor={};"
             " max_readahead={};"
             " flags=0x%016lx (",
             arg_->major,
             arg_->minor,
             arg_->max_readahead,
             flags);
  for(uint64_t i = 0; i < (sizeof(flags)*8); i++)
    {
      const char *str;

      if(!(flags & (1ULL << i)))
        continue;

      str = fuse_flag_to_str(i);
      if(str == NULL)
        continue;

      fmt::print(g_OUTPUT,"%s, ",str);
    }
  fmt::print(g_OUTPUT,")\n");
}

void
fuse_syslog_fuse_init_in(const struct fuse_init_in *arg_)
{
  uint64_t flags;
  std::string output;

  flags = (((uint64_t)arg_->flags) | ((uint64_t)arg_->flags2) << 32);
  output = fmt::format("fuse_init_in:"
                       " major={};"
                       " minor={};"
                       " flags=(",
                       arg_->major,
                       arg_->minor);

  for(uint64_t i = 0; i < (sizeof(flags)*8); i++)
    {
      if(!(flags & (1ULL << i)))
        continue;

      output += fuse_debug_init_flag_name(1ULL << i);

      output += ",";
    }

  output.pop_back();
  output += ");";

  SysLog::info(output);
}

void
fuse_syslog_fuse_init_out(const struct fuse_init_out *arg_)
{
  uint64_t flags;
  std::string output;

  flags = (((uint64_t)arg_->flags) | ((uint64_t)arg_->flags2) << 32);
  output = fmt::format("fuse_init_out:"
                       " major={};"
                       " minor={};"
                       " max_readahead={};"
                       " max_background={};"
                       " congestion_threshold={};"
                       " max_write={};"
                       " time_gran={};"
                       " max_pages={};"
                       " map_alignment={};"
                       " max_stack_depth={};"
                       " flags=(",
                       arg_->major,
                       arg_->minor,
                       arg_->max_readahead,
                       arg_->max_background,
                       arg_->congestion_threshold,
                       arg_->max_write,
                       arg_->time_gran,
                       arg_->max_pages,
                       arg_->map_alignment,
                       arg_->max_stack_depth);

  for(uint64_t i = 0; i < (sizeof(flags)*8); i++)
    {
      if(!(flags & (1ULL << i)))
        continue;

      output += fuse_debug_init_flag_name(1ULL << i);

      output += ",";
    }

  output.pop_back();
  output += ");";

  SysLog::info(output);
}


void
debug_fuse_init_out(const uint64_t              unique_,
                    const struct fuse_init_out *arg_,
                    const uint64_t              argsize_)
{
  uint64_t flags;
  const struct fuse_init_out *arg = arg_;

  flags = (((uint64_t)arg->flags) | ((uint64_t)arg->flags2) << 32);
  fmt::print(g_OUTPUT,
             "FUSE_INIT_OUT:"
             " major={};"
             " minor={};"
             " max_readahead={};"
             " flags=0x%016lx ("
             ,
             /* unique_, */
             /* sizeof(struct fuse_out_header) + argsize_, */
             arg->major,
             arg->minor,
             arg->max_readahead,
             flags);

  for(uint64_t i = 0; i < (sizeof(flags)*8); i++)
    {
      const char *str;

      if(!(flags & (1ULL << i)))
        continue;

      str = fuse_flag_to_str(i);
      if(str == NULL)
        continue;

      fmt::print(g_OUTPUT,"%s, ",str);
    }

  fmt::print(g_OUTPUT,
             "); max_background={};"
             " congestion_threshold={};"
             " max_write={};"
             " time_gran={};"
             " max_pages={};"
             " map_alignment={};"
             "\n",
             arg->max_background,
             arg->congestion_threshold,
             arg->max_write,
             arg->time_gran,
             arg->max_pages,
             arg->map_alignment);
}

static
void
debug_fuse_attr(const struct fuse_attr *attr_)
{
  fmt::print(g_OUTPUT,
             "attr:"
             " ino=0x%016" PRIx64 ";"
             " size={};"
             " blocks={};"
             " atime={};"
             " atimensec={};"
             " mtime={};"
             " mtimensec={};"
             " ctime={};"
             " ctimesec={};"
             " mode={:o};"
             " nlink={};"
             " uid={};"
             " gid={};"
             " rdev={};"
             " blksize={};"
             ,
             attr_->ino,
             attr_->size,
             attr_->blocks,
             attr_->atime,
             attr_->atimensec,
             attr_->mtime,
             attr_->mtimensec,
             attr_->ctime,
             attr_->ctimensec,
             attr_->mode,
             attr_->nlink,
             attr_->uid,
             attr_->gid,
             attr_->rdev,
             attr_->blksize);
}

static
void
debug_fuse_entry(const struct fuse_entry_out *entry_)
{
  fmt::print(g_OUTPUT,
             " fuse_entry_out:"
             " nodeid=0x%016" PRIx64 ";"
             " generation=0x%016" PRIx64 ";"
             " entry_valid={};"
             " entry_valid_nsec={};"
             " attr_valid={};"
             " attr_valid_nsec={};"
             " ",
             entry_->nodeid,
             entry_->generation,
             entry_->entry_valid,
             entry_->entry_valid_nsec,
             entry_->attr_valid,
             entry_->attr_valid_nsec);
  debug_fuse_attr(&entry_->attr);
}

void
debug_fuse_entry_out(const uint64_t               unique_,
                     const struct fuse_entry_out *arg_,
                     const uint64_t               argsize_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             ,
             unique_,
             sizeof(struct fuse_out_header) + argsize_);
  debug_fuse_entry(arg_);

}

void
debug_fuse_attr_out(const uint64_t              unique_,
                    const struct fuse_attr_out *arg_,
                    const uint64_t              argsize_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             "fuse_attr_out:"
             " attr_valid={};"
             " attr_valid_nsec={};"
             " ino={};"
             " size={};"
             " blocks={};"
             " atime={};"
             " atimensec={};"
             " mtime={};"
             " mtimensec={};"
             " ctime={};"
             " ctimensec={};"
             " mode={:o};"
             " nlink={};"
             " uid={};"
             " gid={};"
             " rdev={};"
             " blksize={};"
             "\n",
             unique_,
             sizeof(struct fuse_out_header) + argsize_,
             arg_->attr_valid,
             arg_->attr_valid_nsec,
             arg_->attr.ino,
             arg_->attr.size,
             arg_->attr.blocks,
             arg_->attr.atime,
             arg_->attr.atimensec,
             arg_->attr.mtime,
             arg_->attr.mtimensec,
             arg_->attr.ctime,
             arg_->attr.ctimensec,
             arg_->attr.mode,
             arg_->attr.nlink,
             arg_->attr.uid,
             arg_->attr.gid,
             arg_->attr.rdev,
             arg_->attr.blksize);
}

static
void
debug_fuse_interrupt_in(const void *arg_)
{
  const struct fuse_interrupt_in *arg = (const fuse_interrupt_in*)arg_;

  fmt::print(g_OUTPUT,
             "fuse_interrupt_in:"
             " unique=0x%016" PRIx64 ";"
             "\n"
             ,
             arg->unique);
}

static
const
char*
opcode_name(enum fuse_opcode op_)
{
  static const char *names[] =
    {
      "INVALID",
      "LOOKUP",
      "FORGET",
      "GETATTR",
      "SETATTR",
      "READLINK",
      "SYMLINK",
      "MKNOD",
      "MKDIR",
      "UNLINK",
      "RMDIR",
      "RENAME",
      "LINK",
      "OPEN",
      "READ",
      "WRITE",
      "STATFS",
      "RELEASE",
      "FSYNC",
      "SETXATTR",
      "GETXATTR",
      "LISTXATTR",
      "REMOVEXATTR",
      "FLUSH",
      "INIT",
      "OPENDIR",
      "READDIR",
      "RELEASEDIR",
      "FSYNCDIR",
      "GETLK",
      "SETLK",
      "SETLKW",
      "ACCESS",
      "CREATE",
      "INTERRUPT",
      "BMAP",
      "DESTROY",
      "IOCTL",
      "POLL",
      "NOTIFY_REPLY",
      "BATCH_FORGET",
      "FALLOCATE",
      "READDIRPLUS",
      "RENAME2",
      "LSEEK",
      "COPY_FILE_RANGE",
      "SETUPMAPPING",
      "REMOVEMAPPING"
    };

  if(op_ >= (sizeof(names) / sizeof(names[0])))
    return "::UNKNOWN::";

  return names[op_];
}

void
debug_fuse_in_header(const struct fuse_in_header *hdr_)
{
  const void *arg = &hdr_[1];

  fmt::print(stderr,
             "unique=0x{:016x}";"
             " opcode=%s ({});"
             " nodeid={};"
             " uid={};"
             " gid={};"
             " pid={}; || ",
             hdr_->unique,
             opcode_name((fuse_opcode)hdr_->opcode),
             hdr_->opcode,
             hdr_->nodeid,
             hdr_->uid,
             hdr_->gid,
             hdr_->pid);

  switch(hdr_->opcode)
    {
    case FUSE_LOOKUP:
      debug_fuse_lookup(arg);
      break;
    case FUSE_INIT:
      debug_fuse_init_in((const fuse_init_in*)arg);
      break;
    case FUSE_GETATTR:
      debug_fuse_getattr_in(arg);
      break;
    case FUSE_SETATTR:
      debug_fuse_setattr_in(arg);
      break;
    case FUSE_ACCESS:
      debug_fuse_access_in(arg);
      break;
    case FUSE_MKNOD:
      debug_fuse_mknod_in(arg);
      break;
    case FUSE_MKDIR:
      debug_fuse_mkdir_in(arg);
      break;
    case FUSE_UNLINK:
      debug_fuse_unlink(arg);
      break;
    case FUSE_RMDIR:
      debug_fuse_rmdir(arg);
      break;
    case FUSE_SYMLINK:
      debug_fuse_symlink(arg);
      break;
    case FUSE_RENAME:
      debug_fuse_rename_in(arg);
      break;
    case FUSE_LINK:
      debug_fuse_link_in(arg);
      break;
    case FUSE_CREATE:
      debug_fuse_create_in(arg);
      break;
    case FUSE_OPEN:
      debug_fuse_open_in(arg);
      break;
    case FUSE_OPENDIR:
      debug_fuse_open_in(arg);
      break;
    case FUSE_READ:
      debug_fuse_read_in(arg);
      break;
    case FUSE_READDIR:
      debug_fuse_read_in(arg);
      break;
    case FUSE_READDIRPLUS:
      debug_fuse_read_in(arg);
      break;
    case FUSE_WRITE:
      debug_fuse_write_in(arg);
      break;
    case FUSE_RELEASE:
      debug_fuse_release_in(arg);
      break;
    case FUSE_RELEASEDIR:
      debug_fuse_release_in(arg);
      break;
    case FUSE_FSYNCDIR:
      debug_fuse_fsync_in(arg);
      break;
    case FUSE_GETXATTR:
      debug_fuse_getxattr_in(arg);
      break;
    case FUSE_LISTXATTR:
      debug_fuse_listxattr(arg);
      break;
    case FUSE_SETXATTR:
      debug_fuse_setxattr_in(arg);
      break;
    case FUSE_REMOVEXATTR:
      debug_fuse_removexattr(arg);
      break;
    case FUSE_FALLOCATE:
      debug_fuse_fallocate_in(arg);
      break;
    case FUSE_FLUSH:
      debug_fuse_flush_in(arg);
      break;
    case FUSE_INTERRUPT:
      debug_fuse_interrupt_in(arg);
      break;
    default:
      fmt::print(g_OUTPUT,"FIXME\n");
      break;
    }
}

void
debug_fuse_out_header(const struct fuse_out_header *hdr_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=%d (%s);"
             " len={};"
             ,
             hdr_->unique,
             hdr_->error,
             strerror(-hdr_->error),
             sizeof(struct fuse_out_header));
}

void
debug_fuse_entry_open_out(const uint64_t               unique_,
                          const struct fuse_entry_out *entry_,
                          const struct fuse_open_out  *open_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             ,
             unique_,
             sizeof(struct fuse_entry_out) + sizeof(struct fuse_open_out));
  debug_fuse_entry(entry_);

}

void
debug_fuse_readlink(const uint64_t  unique_,
                    const char     *linkname_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             "readlink: linkname=%s"
             "\n"
             ,
             unique_,
             (sizeof(struct fuse_out_header) + strlen(linkname_)),
             linkname_);
}

void
debug_fuse_write_out(const uint64_t               unique_,
                     const struct fuse_write_out *arg_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             " fuse_write_out:"
             " size={}"
             "\n"
             ,
             unique_,
             sizeof(struct fuse_write_out),
             arg_->size);
}

void
debug_fuse_statfs_out(const uint64_t                unique_,
                      const struct fuse_statfs_out *arg_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             " fuse_statfs_out:"
             " blocks={};"
             " bfree={};"
             " bavail={};"
             " files={};"
             " ffree={};"
             " bsize={};"
             " namelen={};"
             " frsize={};"
             "\n"
             ,
             unique_,
             sizeof(struct fuse_statfs_out),
             arg_->st.blocks,
             arg_->st.bfree,
             arg_->st.bavail,
             arg_->st.files,
             arg_->st.ffree,
             arg_->st.bsize,
             arg_->st.namelen,
             arg_->st.frsize);
}

void
debug_fuse_getxattr_out(const uint64_t            unique_,
                        const struct fuse_getxattr_out *arg_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             " fuse_getxattr_out:"
             " size={};"
             "\n"
             ,
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_getxattr_out),
             arg_->size);

}

void
debug_fuse_lk_out(const uint64_t            unique_,
                  const struct fuse_lk_out *arg_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             " fuse_file_lock:"
             " start={};"
             " end={};"
             " type={};"
             " pid={};"
             "\n"
             ,
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_lk_out),
             arg_->lk.start,
             arg_->lk.end,
             arg_->lk.type,
             arg_->lk.pid);
}

void
debug_fuse_bmap_out(const uint64_t              unique_,
                    const struct fuse_bmap_out *arg_)
{
  fmt::print(g_OUTPUT,
             "unique=0x%016" PRIx64 ";"
             " opcode=RESPONSE;"
             " error=0 (Success);"
             " len={}; || "
             " fuse_bmap_out:"
             " block={};"
             "\n"
             ,
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_bmap_out),
             arg_->block);

}

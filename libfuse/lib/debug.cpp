/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_cfg.hpp"
#include "fuse_kernel.h"

#include "fmt/core.h"
#include "fmt/format.h"

#include <memory>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PARAM(inarg) (((char *)(inarg)) + sizeof(*(inarg)))

static
void
_fclose_deleter(FILE *f_)
{
  if(f_ == nullptr)
    return;
  if(f_ == stdin)
    return;
  if(f_ == stdout)
    return;
  if(f_ == stderr)
    return;

  fclose(f_);
}

static
int
_debug_set_output_stderr()
{
  auto new_log_file = std::shared_ptr<FILE>(stderr,[](FILE*){});

  fuse_cfg.log_file(new_log_file);
  fuse_cfg.log_filepath("/dev/stderr");

  return 0;
}

static
int
_debug_set_output_filepath(const std::string &filepath_)
{
  FILE *tmp_new_file;

  tmp_new_file = fopen(filepath_.c_str(),"a");
  if(tmp_new_file == NULL)
    return -errno;

  setvbuf(tmp_new_file,NULL,_IOLBF,0);

  auto new_log_file =
    std::shared_ptr<FILE>(tmp_new_file,::_fclose_deleter);

  fuse_cfg.log_file(new_log_file);
  fuse_cfg.log_filepath(filepath_);

  return 0;
}

int
fuse_debug_set_output(const std::string &filepath_)
{
  if(filepath_.empty())
    return _debug_set_output_stderr();

  return _debug_set_output_filepath(filepath_.c_str());
}

static
uint64_t
_timestamp_ns(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC,&ts);

  return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static
std::string
_quoted(const char *str_)
{
  std::string out;

  out += '"';
  for(const char *p = str_; *p; p++)
    {
      if(*p == '"' || *p == '\\')
        out += '\\';
      out += *p;
    }
  out += '"';

  return out;
}

static
std::string
_quoted(const char *data_,
        size_t      len_)
{
  std::string out;

  out += '"';
  for(size_t i = 0; i < len_; i++)
    {
      if(data_[i] == '"' || data_[i] == '\\')
        out += '\\';
      out += data_[i];
    }
  out += '"';

  return out;
}

static
const
char*
_open_accmode_to_str(const int flags_)
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
_fuse_write_flag_to_str(const uint32_t offset_)
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
_open_flag_to_str(const uint64_t offset_)
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
_fuse_flag_to_str(const uint64_t offset_)
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
std::string
_open_flags_str(const uint32_t flags_)
{
  std::string buf;

  buf += _open_accmode_to_str(flags_);
  for(size_t i = 0; i < (sizeof(flags_) * 8); i++)
    {
      const char *str;

      if(!(flags_ & (1 << i)))
        continue;

      str = _open_flag_to_str(i);
      if(str == NULL)
        continue;

      buf += '|';
      buf += str;
    }

  return buf;
}

#define FOPEN_FLAG_CASE(X) case FOPEN_##X: return #X

static
const
char*
_fuse_fopen_flag_to_str(const uint32_t offset_)
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

static
std::string
_fopen_flags_str(const uint32_t flags_)
{
  std::string buf;

  for(size_t i = 0; i < (sizeof(flags_) * 8); i++)
    {
      const char *str;

      if(!(flags_ & (1 << i)))
        continue;

      str = _fuse_fopen_flag_to_str(i);
      if(str == NULL)
        continue;

      if(!buf.empty())
        buf += '|';
      buf += str;
    }

  return buf;
}

static
std::string
_write_flags_str(const uint32_t flags_)
{
  std::string buf;

  for(size_t i = 0; i < (sizeof(flags_) * 8); i++)
    {
      const char *str;

      if(!(flags_ & (1 << i)))
        continue;

      str = _fuse_write_flag_to_str(i);
      if(str == NULL)
        continue;

      if(!buf.empty())
        buf += '|';
      buf += str;
    }

  return buf;
}

static
std::string
_fuse_init_flags_str(const uint64_t flags_)
{
  std::string buf;

  for(uint64_t i = 0; i < (sizeof(flags_)*8); i++)
    {
      const char *str;

      if(!(flags_ & (1ULL << i)))
        continue;

      str = _fuse_flag_to_str(i);
      if(str == NULL)
        continue;

      if(!buf.empty())
        buf += '|';
      buf += str;
    }

  return buf;
}

static
std::string
_xattr_value_to_printable(const char *data_,
                          uint32_t    size_)
{
  bool is_printable = true;

  for(uint32_t i = 0; i < size_; i++)
    {
      unsigned char c = (unsigned char)data_[i];
      if(c < 0x20 || c > 0x7E)
        {
          is_printable = false;
          break;
        }
    }

  if(is_printable)
    return _quoted(data_,size_);

  std::string hex;
  hex += '"';
  for(uint32_t i = 0; i < size_; i++)
    {
      if(i > 0)
        hex += ' ';
      hex += fmt::format("{:02X}",(unsigned char)data_[i]);
    }
  hex += '"';

  return hex;
}

void
fuse_debug_open_out(const uint64_t              unique_,
                    const struct fuse_open_out *arg_,
                    const uint64_t              argsize_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " fh=0x{:08X}"
             " open_flags=0x{:04X}"
             " open_flags_str=\"{}\""
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + argsize_,
             arg_->fh,
             arg_->open_flags,
             _fopen_flags_str(arg_->open_flags));
}

static
void
_debug_fuse_lookup(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name = (const char*)arg_;

  fmt::print(output.get(),
             "name={}\n",
             ::              _quoted(name));
}

static
void
_debug_fuse_getattr_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_getattr_in *arg = (const fuse_getattr_in*)arg_;

  fmt::print(output.get(),
             "getattr_flags=0x{:08X}"
             " fh=0x{:08X}"
             "\n",
             arg->getattr_flags,
             arg->fh);
}

static
void
_debug_fuse_setattr_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_setattr_in *arg = (const fuse_setattr_in*)arg_;

  fmt::print(output.get(),
             "valid={}"
             " fh=0x{:08X}"
             " size={}"
             " lock_owner={}"
             " atime={}"
             " atimensec={}"
             " mtime={}"
             " mtimensec={}"
             " ctime={}"
             " ctimensec={}"
             " mode={:o}"
             " uid={}"
             " gid={}"
             "\n",
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
_debug_fuse_access_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_access_in *arg = (const fuse_access_in*)arg_;

  fmt::print(output.get(),
             "mask=0x{:08X}"
             "\n",
             arg->mask);
}

static
void
_debug_fuse_mknod_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_mknod_in *arg = (const fuse_mknod_in*)arg_;

  fmt::print(output.get(),
             "mode={:o}"
             " rdev=0x{:08X}"
             " umask={:o}"
             " name={}"
             "\n",
             arg->mode,
             arg->rdev,
             arg->umask,
             _quoted(PARAM(arg)));
}

static
void
_debug_fuse_mkdir_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_mkdir_in *arg = (const fuse_mkdir_in*)arg_;

  fmt::print(output.get(),
             "mode={:o}"
             " umask={:o}"
             " name={}"
             "\n",
             arg->mode,
             arg->umask,
             _quoted(PARAM(arg)));
}

static
void
_debug_fuse_unlink(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name = (const char*)arg_;

  fmt::print(output.get(),
             "name={}"
             "\n",
             ::              _quoted(name));
}

static
void
_debug_fuse_rmdir(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name = (const char*)arg_;

  fmt::print(output.get(),
             "name={}"
             "\n",
             ::              _quoted(name));
}

static
void
_debug_fuse_symlink(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const char *linkname;

  name     = (const char*)arg_;
  linkname = (name + (strlen(name) + 1));

  fmt::print(output.get(),
             "name={}"
             " linkname={}"
             "\n",
             _quoted(name),
             _quoted(linkname));
}

static
void
_debug_fuse_rename_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *oldname;
  const char *newname;
  const struct fuse_rename_in *arg = (const fuse_rename_in*)arg_;

  oldname = PARAM(arg);
  newname = (oldname + strlen(oldname) + 1);

  fmt::print(output.get(),
             "oldname={}"
             " newdir={}"
             " newname={}"
             "\n",
             _quoted(oldname),
             arg->newdir,
             _quoted(newname));
}

static
void
_debug_fuse_link_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const struct fuse_link_in *arg = (const fuse_link_in*)arg_;

  name = PARAM(arg);

  fmt::print(output.get(),
             "oldnodeid={}"
             " name={}"
             "\n",
             arg->oldnodeid,
             ::              _quoted(name));
}

static
void
_debug_fuse_create_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const struct fuse_create_in *arg = (const fuse_create_in*)arg_;

  name = PARAM(arg);

  fmt::print(output.get(),
             "mode={:o}"
             " umask={:o}"
             " name={}"
             " flags=0x{:X}"
             " flags_str=\"{}\""
             "\n",
             arg->mode,
             arg->umask,
             _quoted(name),
             arg->flags,
             _open_flags_str(arg->flags));
}

static
void
_debug_fuse_open_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_open_in *arg = (const fuse_open_in*)arg_;

  fmt::print(output.get(),
             "flags=0x{:08X}"
             " flags_str=\"{}\""
             "\n",
             arg->flags,
             _open_flags_str(arg->flags));
}

static
void
_debug_fuse_read_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_read_in *arg = (const fuse_read_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " offset={}"
             " size={}"
             " read_flags=0x{:X}"
             " lock_owner=0x{:08X}"
             " flags=0x{:X}"
             " flags_str=\"{}\""
             "\n",
             arg->fh,
             arg->offset,
             arg->size,
             arg->read_flags,
             arg->lock_owner,
             arg->flags,
             _open_flags_str(arg->flags));
}

static
void
_debug_fuse_write_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_write_in *arg = (const fuse_write_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " offset={}"
             " size={}"
             " lock_owner=0x{:08X}"
             " flags=0x{:X}"
             " flags_str=\"{}\""
             " write_flags=0x{:X}"
             " write_flags_str=\"{}\""
             "\n",
             arg->fh,
             arg->offset,
             arg->size,
             arg->lock_owner,
             arg->flags,
             _open_flags_str(arg->flags),
             arg->write_flags,
             _write_flags_str(arg->write_flags));
}

static
void
_debug_fuse_flush_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_flush_in *arg = (const fuse_flush_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " lock_owner=0x{:08X}"
             "\n",
             arg->fh,
             arg->lock_owner);
}

static
void
_debug_fuse_release_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_release_in *arg = (const fuse_release_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " release_flags=0x{:X}"
             " lock_owner=0x{:08X}"
             " flags=0x{:X}"
             " flags_str=\"{}\""
             "\n",
             arg->fh,
             arg->release_flags,
             arg->lock_owner,
             arg->flags,
             _open_flags_str(arg->flags));
}

static
void
_debug_fuse_fsync_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_fsync_in *arg = (const fuse_fsync_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " fsync_flags=0x{:X}"
             "\n",
             arg->fh,
             arg->fsync_flags);
}

static
void
_debug_fuse_setxattr_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const char *value;
  const struct fuse_setxattr_in *arg = (const fuse_setxattr_in*)arg_;

  name  = PARAM(arg);
  value = (name + strlen(name) + 1);

  fmt::print(output.get(),
             "size={}"
             " flags=0x{:X}"
             " name={}"
             " value={}"
             "\n",
             arg->size,
             arg->flags,
             _quoted(name),
             _xattr_value_to_printable(value,arg->size));
}

static
void
_debug_fuse_getxattr_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const struct fuse_getxattr_in *arg = (const fuse_getxattr_in*)arg_;

  name = PARAM(arg);

  fmt::print(output.get(),
             "size={}"
             " name={}"
             "\n",
             arg->size,
             ::              _quoted(name));
}

static
void
_debug_fuse_listxattr(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_getxattr_in *arg = (const fuse_getxattr_in*)arg_;

  fmt::print(output.get(),
             "size={}"
             "\n",
             arg->size);
}

static
void
_debug_fuse_removexattr(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name = (const char*)arg_;

  fmt::print(output.get(),
             "name={}"
             "\n",
             ::              _quoted(name));
}

static
void
_debug_fuse_fallocate_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_fallocate_in *arg = (const fuse_fallocate_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " offset={}"
             " length={}"
             " mode={:o}"
             "\n",
             arg->fh,
             arg->offset,
             arg->length,
             arg->mode);
}

static
void
_debug_fuse_forget_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_forget_in *arg = (const fuse_forget_in*)arg_;

  fmt::print(output.get(),
             "nlookup={}"
             "\n",
             arg->nlookup);
}

static
void
_debug_fuse_batch_forget_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_batch_forget_in *arg = (const fuse_batch_forget_in*)arg_;
  const struct fuse_forget_one *items = (const fuse_forget_one*)PARAM(arg);

  fmt::print(output.get(),
             "count={}",
             arg->count);
  for(uint32_t i = 0; i < arg->count; i++)
    {
      fmt::print(output.get(),
                 " item.{}={{nodeid={},nlookup={}}}",
                 i,
                 items[i].nodeid,
                 items[i].nlookup);
    }
  fmt::print(output.get(),"\n");
}

static
void
_debug_fuse_lk_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_lk_in *arg = (const fuse_lk_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " owner=0x{:016X}"
             " lk_start={}"
             " lk_end={}"
             " lk_type={}"
             " lk_pid={}"
             " lk_flags=0x{:X}"
             "\n",
             arg->fh,
             arg->owner,
             arg->lk.start,
             arg->lk.end,
             arg->lk.type,
             arg->lk.pid,
             arg->lk_flags);
}

static
void
_debug_fuse_bmap_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_bmap_in *arg = (const fuse_bmap_in*)arg_;

  fmt::print(output.get(),
             "block={}"
             " blocksize={}"
             "\n",
             arg->block,
             arg->blocksize);
}

static
void
_debug_fuse_ioctl_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_ioctl_in *arg = (const fuse_ioctl_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " flags=0x{:X}"
             " cmd=0x{:X}"
             " arg=0x{:016X}"
             " in_size={}"
             " out_size={}"
             "\n",
             arg->fh,
             arg->flags,
             arg->cmd,
             arg->arg,
             arg->in_size,
             arg->out_size);
}

static
void
_debug_fuse_poll_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_poll_in *arg = (const fuse_poll_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " kh=0x{:016X}"
             " flags=0x{:X}"
             " events=0x{:X}"
             "\n",
             arg->fh,
             arg->kh,
             arg->flags,
             arg->events);
}

static
void
_debug_fuse_rename2_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *oldname;
  const char *newname;
  const struct fuse_rename2_in *arg = (const fuse_rename2_in*)arg_;

  oldname = PARAM(arg);
  newname = (oldname + strlen(oldname) + 1);

  fmt::print(output.get(),
             "oldname={}"
             " newdir={}"
             " newname={}"
             " flags=0x{:X}"
             "\n",
             _quoted(oldname),
             arg->newdir,
             _quoted(newname),
             arg->flags);
}

static
void
_debug_fuse_lseek_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_lseek_in *arg = (const fuse_lseek_in*)arg_;

  fmt::print(output.get(),
             "fh=0x{:08X}"
             " offset={}"
             " whence={}"
             "\n",
             arg->fh,
             arg->offset,
             arg->whence);
}

static
void
_debug_fuse_copy_file_range_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_copy_file_range_in *arg = (const fuse_copy_file_range_in*)arg_;

  fmt::print(output.get(),
             "fh_in=0x{:08X}"
             " off_in={}"
             " nodeid_out={}"
             " fh_out=0x{:08X}"
             " off_out={}"
             " len={}"
             " flags=0x{:X}"
             "\n",
             arg->fh_in,
             arg->off_in,
             arg->nodeid_out,
             arg->fh_out,
             arg->off_out,
             arg->len,
             arg->flags);
}

static
void
_debug_fuse_statx_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_statx_in *arg = (const fuse_statx_in*)arg_;

  fmt::print(output.get(),
             "getattr_flags=0x{:08X}"
             " fh=0x{:08X}"
             " sx_flags=0x{:X}"
             " sx_mask=0x{:X}"
             "\n",
             arg->getattr_flags,
             arg->fh,
             arg->sx_flags,
             arg->sx_mask);
}

static
void
_debug_fuse_tmpfile_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const char *name;
  const struct fuse_create_in *arg = (const fuse_create_in*)arg_;

  name = PARAM(arg);

  fmt::print(output.get(),
             "mode={:o}"
             " umask={:o}"
             " name={}"
             " flags=0x{:08X}"
             " flags_str=\"{}\""
             "\n",
             arg->mode,
             arg->umask,
             _quoted(name),
             arg->flags,
             _open_flags_str(arg->flags));
}

static
void
_debug_init_in(const struct fuse_init_in *arg_)
{
  auto output = fuse_cfg.log_file();

  uint64_t flags;

  flags = (((uint64_t)arg_->flags) | ((uint64_t)arg_->flags2) << 32);
  fmt::print(output.get(),
             "major={}"
             " minor={}"
             " max_readahead={}"
             " flags=0x{:016X}"
             " flags_str=\"{}\""
             "\n",
             arg_->major,
             arg_->minor,
             arg_->max_readahead,
             flags,
             _fuse_init_flags_str(flags));
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
fuse_debug_init_out(const uint64_t              unique_,
                    const struct fuse_init_out *arg_,
                    const uint64_t              argsize_)
{
  auto output = fuse_cfg.log_file();

  uint64_t flags;
  const struct fuse_init_out *arg = arg_;

  flags = (((uint64_t)arg->flags) | ((uint64_t)arg->flags2) << 32);
  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " major={}"
             " minor={}"
             " max_readahead={}"
             " flags=0x{:016X}"
             " flags_str=\"{}\""
             " max_background={}"
             " congestion_threshold={}"
             " max_write={}"
             " time_gran={}"
             " max_pages={}"
             " map_alignment={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + argsize_,
             arg->major,
             arg->minor,
             arg->max_readahead,
             flags,
             _fuse_init_flags_str(flags),
             arg->max_background,
             arg->congestion_threshold,
             arg->max_write,
             arg->time_gran,
             arg->max_pages,
             arg->map_alignment);
}

static
void
_debug_fuse_attr(const struct fuse_attr *attr_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ino=0x{:016X}"
             " size={}"
             " blocks={}"
             " atime={}"
             " atimensec={}"
             " mtime={}"
             " mtimensec={}"
             " ctime={}"
             " ctimensec={}"
             " mode={:o}"
             " nlink={}"
             " uid={}"
             " gid={}"
             " rdev={}"
             " blksize={}",
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
_debug_fuse_entry(const struct fuse_entry_out *entry_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "nodeid=0x{:016X}"
             " generation=0x{:016X}"
             " entry_valid={}"
             " entry_valid_nsec={}"
             " attr_valid={}"
             " attr_valid_nsec={}"
             " ",
             entry_->nodeid,
             entry_->generation,
             entry_->entry_valid,
             entry_->entry_valid_nsec,
             entry_->attr_valid,
             entry_->attr_valid_nsec);
  _debug_fuse_attr(&entry_->attr);
}

void
fuse_debug_entry_out(const uint64_t               unique_,
                     const struct fuse_entry_out *arg_,
                     const uint64_t               argsize_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " ",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + argsize_);
  _debug_fuse_entry(arg_);
  fmt::print(output.get(),"\n");
}

void
fuse_debug_attr_out(const uint64_t              unique_,
                    const struct fuse_attr_out *arg_,
                    const uint64_t              argsize_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " attr_valid={}"
             " attr_valid_nsec={}"
             " ino=0x{:016X}"
             " size={}"
             " blocks={}"
             " atime={}"
             " atimensec={}"
             " mtime={}"
             " mtimensec={}"
             " ctime={}"
             " ctimensec={}"
             " mode={:o}"
             " nlink={}"
             " uid={}"
             " gid={}"
             " rdev={}"
             " blksize={}"
             "\n",
             _timestamp_ns(),
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
_debug_fuse_interrupt_in(const void *arg_)
{
  auto output = fuse_cfg.log_file();

  const struct fuse_interrupt_in *arg = (const fuse_interrupt_in*)arg_;

  fmt::print(output.get(),
             "target_unique=0x{:016X}"
             "\n",
             arg->unique);
}

static
const
char*
_opcode_name(enum fuse_opcode op_)
{
  static const char *names[] =
    {
      "RESERVED",
      "LOOKUP",
      "FORGET",
      "GETATTR",
      "SETATTR",
      "READLINK",
      "SYMLINK",
      "RESERVED",
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
      "RESERVED",
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
      "REMOVEMAPPING",
      "SYNCFS",
      "TMPFILE",
      "STATX"
    };

  if(op_ >= (sizeof(names) / sizeof(names[0])))
    return "UNKNOWN";

  return names[op_];
}

void
fuse_debug_in_header(const struct fuse_in_header *hdr_)
{
  const void *arg = &hdr_[1];
  auto output = fuse_cfg.log_file();

  flockfile(output.get());

  fmt::print(output.get(),
             "ts={}"
             " dir=IN"
             " unique=0x{:016X}"
             " opcode={}"
             " opcode_id={}"
             " nodeid={}"
             " uid={}"
             " gid={}"
             " pid={}"
             " ",
             _timestamp_ns(),
             hdr_->unique,
             _opcode_name((fuse_opcode)hdr_->opcode),
             hdr_->opcode,
             hdr_->nodeid,
             hdr_->uid,
             hdr_->gid,
             hdr_->pid);

  switch(hdr_->opcode)
    {
    case FUSE_LOOKUP:
      _debug_fuse_lookup(arg);
      break;
    case FUSE_FORGET:
      _debug_fuse_forget_in(arg);
      break;
    case FUSE_GETATTR:
      _debug_fuse_getattr_in(arg);
      break;
    case FUSE_SETATTR:
      _debug_fuse_setattr_in(arg);
      break;
    case FUSE_READLINK:
      fmt::print(output.get(),"\n");
      break;
    case FUSE_SYMLINK:
      _debug_fuse_symlink(arg);
      break;
    case FUSE_UNLINK:
      _debug_fuse_unlink(arg);
      break;
    case FUSE_RMDIR:
      _debug_fuse_rmdir(arg);
      break;
    case FUSE_MKNOD:
      _debug_fuse_mknod_in(arg);
      break;
    case FUSE_MKDIR:
      _debug_fuse_mkdir_in(arg);
      break;
    case FUSE_RENAME:
      _debug_fuse_rename_in(arg);
      break;
    case FUSE_LINK:
      _debug_fuse_link_in(arg);
      break;
    case FUSE_OPEN:
      _debug_fuse_open_in(arg);
      break;
    case FUSE_READ:
      _debug_fuse_read_in(arg);
      break;
    case FUSE_WRITE:
      _debug_fuse_write_in(arg);
      break;
    case FUSE_STATFS:
      fmt::print(output.get(),"\n");
      break;
    case FUSE_RELEASE:
      _debug_fuse_release_in(arg);
      break;
    case FUSE_FSYNC:
      _debug_fuse_fsync_in(arg);
      break;
    case FUSE_SETXATTR:
      _debug_fuse_setxattr_in(arg);
      break;
    case FUSE_GETXATTR:
      _debug_fuse_getxattr_in(arg);
      break;
    case FUSE_LISTXATTR:
      _debug_fuse_listxattr(arg);
      break;
    case FUSE_REMOVEXATTR:
      _debug_fuse_removexattr(arg);
      break;
    case FUSE_FLUSH:
      _debug_fuse_flush_in(arg);
      break;
    case FUSE_OPENDIR:
      _debug_fuse_open_in(arg);
      break;
    case FUSE_READDIR:
      _debug_fuse_read_in(arg);
      break;
    case FUSE_RELEASEDIR:
      _debug_fuse_release_in(arg);
      break;
    case FUSE_FSYNCDIR:
      _debug_fuse_fsync_in(arg);
      break;
    case FUSE_GETLK:
      _debug_fuse_lk_in(arg);
      break;
    case FUSE_SETLK:
      _debug_fuse_lk_in(arg);
      break;
    case FUSE_SETLKW:
      _debug_fuse_lk_in(arg);
      break;
    case FUSE_INTERRUPT:
      _debug_fuse_interrupt_in(arg);
      break;
    case FUSE_BMAP:
      _debug_fuse_bmap_in(arg);
      break;
    case FUSE_CREATE:
      _debug_fuse_create_in(arg);
      break;
    case FUSE_IOCTL:
      _debug_fuse_ioctl_in(arg);
      break;
    case FUSE_POLL:
      _debug_fuse_poll_in(arg);
      break;
    case FUSE_BATCH_FORGET:
      _debug_fuse_batch_forget_in(arg);
      break;
    case FUSE_FALLOCATE:
      _debug_fuse_fallocate_in(arg);
      break;
    case FUSE_READDIRPLUS:
      _debug_fuse_read_in(arg);
      break;
    case FUSE_RENAME2:
      _debug_fuse_rename2_in(arg);
      break;
    case FUSE_LSEEK:
      _debug_fuse_lseek_in(arg);
      break;
    case FUSE_COPY_FILE_RANGE:
      _debug_fuse_copy_file_range_in(arg);
      break;
    case FUSE_TMPFILE:
      _debug_fuse_tmpfile_in(arg);
      break;
    case FUSE_STATX:
      _debug_fuse_statx_in(arg);
      break;
    case FUSE_ACCESS:
      _debug_fuse_access_in(arg);
      break;
    case FUSE_INIT:
      _debug_init_in((const fuse_init_in*)arg);
      break;
    default:
      fmt::print(output.get(),"unknown_opcode={}\n",hdr_->opcode);
      break;
    }

  funlockfile(output.get());
}

void
fuse_debug_out_header(const struct fuse_out_header *hdr_)
{
  auto output = fuse_cfg.log_file();

  char strerror_buf[128];

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error={}"
             " error_str=\"{}\""
             " len={}"
             "\n",
             _timestamp_ns(),
             hdr_->unique,
             hdr_->error,
             strerror_r(-hdr_->error,strerror_buf,sizeof(strerror_buf)),
             hdr_->len);
}

void
fuse_debug_entry_open_out(const uint64_t               unique_,
                          const struct fuse_entry_out *entry_,
                          const struct fuse_open_out  *open_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " ",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_entry_out) + sizeof(struct fuse_open_out));
  _debug_fuse_entry(entry_);
  fmt::print(output.get(),
             " fh=0x{:08X}"
             " open_flags=0x{:04X}"
             " open_flags_str=\"{}\""
             "\n",
             open_->fh,
             open_->open_flags,
             _fopen_flags_str(open_->open_flags));
}

void
fuse_debug_readlink(const uint64_t  unique_,
                    const char     *linkname_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " linkname={}"
             "\n",
             _timestamp_ns(),
             unique_,
             (sizeof(struct fuse_out_header) + strlen(linkname_)),
             _quoted(linkname_));
}

void
fuse_debug_write_out(const uint64_t               unique_,
                     const struct fuse_write_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " size={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_write_out),
             arg_->size);
}

void
fuse_debug_statfs_out(const uint64_t                unique_,
                      const struct fuse_statfs_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " blocks={}"
             " bfree={}"
             " bavail={}"
             " files={}"
             " ffree={}"
             " bsize={}"
             " namelen={}"
             " frsize={}"
             "\n",
             _timestamp_ns(),
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
fuse_debug_getxattr_out(const uint64_t            unique_,
                        const struct fuse_getxattr_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " size={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_getxattr_out),
             arg_->size);
}

void
fuse_debug_lk_out(const uint64_t            unique_,
                  const struct fuse_lk_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " lk_start={}"
             " lk_end={}"
             " lk_type={}"
             " lk_pid={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_lk_out),
             arg_->lk.start,
             arg_->lk.end,
             arg_->lk.type,
             arg_->lk.pid);
}

void
fuse_debug_bmap_out(const uint64_t              unique_,
                    const struct fuse_bmap_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " block={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_bmap_out),
             arg_->block);
}

void
fuse_debug_statx_out(const uint64_t               unique_,
                     const struct fuse_statx_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " attr_valid={}"
             " attr_valid_nsec={}"
             " flags=0x{:X}"
             " ino=0x{:016X}"
             " size={}"
             " blocks={}"
             " mode={:o}"
             " nlink={}"
             " uid={}"
             " gid={}"
             " blksize={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_statx_out),
             arg_->attr_valid,
             arg_->attr_valid_nsec,
             arg_->flags,
             arg_->stat.ino,
             arg_->stat.size,
             arg_->stat.blocks,
             arg_->stat.mode,
             arg_->stat.nlink,
             arg_->stat.uid,
             arg_->stat.gid,
             arg_->stat.blksize);
}

void
fuse_debug_data_out(const uint64_t unique_,
                    const size_t   size_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " data_size={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + size_,
             size_);
}

void
fuse_debug_ioctl_out(const uint64_t               unique_,
                     const struct fuse_ioctl_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " result={}"
             " flags=0x{:X}"
             " in_iovs={}"
             " out_iovs={}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_ioctl_out),
             arg_->result,
             arg_->flags,
             arg_->in_iovs,
             arg_->out_iovs);
}

void
fuse_debug_poll_out(const uint64_t              unique_,
                    const struct fuse_poll_out *arg_)
{
  auto output = fuse_cfg.log_file();

  fmt::print(output.get(),
             "ts={}"
             " dir=OUT"
             " unique=0x{:016X}"
             " error=0"
             " len={}"
             " revents=0x{:X}"
             "\n",
             _timestamp_ns(),
             unique_,
             sizeof(struct fuse_out_header) + sizeof(struct fuse_poll_out),
             arg_->revents);
}

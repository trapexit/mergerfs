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

#include "func_open_create_utils.hpp"

#include "procfs_get_name.hpp"

/*
  The kernel expects being able to issue read requests when running
  with writeback caching enabled so we must change O_WRONLY to
  O_RDWR.

  With writeback caching enabled the kernel handles O_APPEND. Could
  be an issue if the underlying file changes out of band but that is
  true of any caching.
*/
void
utils::tweak_flags_writeback_cache(int *flags_)
{
  if((*flags_ & O_ACCMODE) == O_WRONLY)
    *flags_ = ((*flags_ & ~O_ACCMODE) | O_RDWR);
  if(*flags_ & O_APPEND)
    *flags_ &= ~O_APPEND;
}

namespace l
{
  static
  bool
  rdonly(const int flags_)
  {
    return ((flags_ & O_ACCMODE) == O_RDONLY);
  }
}

bool
utils::calculate_flush(FlushOnClose flushonclose_,
                       int const    flags_)
{
  switch(flushonclose_)
    {
    case FlushOnCloseEnum::NEVER:
      return false;
    case FlushOnCloseEnum::OPENED_FOR_WRITE:
      return !l::rdonly(flags_);
    case FlushOnCloseEnum::ALWAYS:
      return true;
    }

  return true;
}


void
utils::cfg_to_ffi_flags(Config::Read     &cfg_,
                        int const         tid_,
                        fuse_file_info_t *ffi_)
{
  switch(cfg_->cache_files)
    {
    case CacheFiles::ENUM::LIBFUSE:
      ffi_->direct_io  = cfg_->direct_io;
      ffi_->keep_cache = cfg_->kernel_cache;
      ffi_->auto_cache = cfg_->auto_cache;
      break;
    case CacheFiles::ENUM::OFF:
      ffi_->direct_io  = 1;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::PARTIAL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::FULL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 1;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::AUTO_FULL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 1;
      break;
    case CacheFiles::ENUM::PER_PROCESS:
      std::string proc_name;

      proc_name = procfs::get_name(tid_);
      if(cfg_->cache_files_process_names.count(proc_name) == 0)
        {
          ffi_->direct_io  = 1;
          ffi_->keep_cache = 0;
          ffi_->auto_cache = 0;
        }
      else
        {
          ffi_->direct_io  = 0;
          ffi_->keep_cache = 0;
          ffi_->auto_cache = 0;
        }
      break;
    }

  if(cfg_->parallel_direct_writes == true)
    ffi_->parallel_direct_writes = ffi_->direct_io;
}

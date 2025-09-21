/*
  ISC License

  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_copyfile.hpp"

#include "fs_attr.hpp"
#include "fs_close.hpp"
#include "fs_copydata.hpp"
#include "fs_fchmod.hpp"
#include "fs_fchown.hpp"
#include "fs_fcntl.hpp"
#include "fs_file_unchanged.hpp"
#include "fs_fstat.hpp"
#include "fs_futimens.hpp"
#include "fs_mktemp.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_rename.hpp"
#include "fs_unlink.hpp"
#include "fs_xattr.hpp"

#include "scope_guard.hpp"

#include <fcntl.h>
#include <signal.h>


static
bool
_ignorable_error(const s64 err_)
{
  switch(err_)
    {
    case ENOTTY:
    case ENOTSUP:
#if ENOTSUP != EOPNOTSUPP
    case EOPNOTSUPP:
#endif
      return true;
    }

  return false;
}

s64
fs::copyfile(const int          src_fd_,
             const struct stat &src_st_,
             const int          dst_fd_)
{
  s64 rv;

  rv = fs::copydata(src_fd_,dst_fd_,src_st_.st_size);
  if(rv < 0)
    return rv;

  rv = fs::xattr::copy(src_fd_,dst_fd_);
  if((rv < 0) && !::_ignorable_error(-rv))
    return rv;

  rv = fs::attr::copy(src_fd_,dst_fd_,FS_ATTR_CLEAR_IMMUTABLE);
  if((rv < 0) && !::_ignorable_error(-rv))
    return rv;

  rv = fs::fchown_check_on_error(dst_fd_,src_st_);
  if(rv < 0)
    return rv;

  rv = fs::fchmod_check_on_error(dst_fd_,src_st_);
  if(rv < 0)
    return rv;

  rv = fs::futimens(dst_fd_,src_st_);
  if(rv < 0)
    return rv;

  return 0;
}

// Limitations:
// * Doesn't handle immutable files well. Will ignore the flag on attr
// copy.
// * Does not handle non-regular files.

s64
fs::copyfile(const int                src_fd_,
             const fs::path          &dst_filepath_,
             const fs::CopyFileFlags &flags_)
{
  s64 rv;
  int dst_fd;
  struct stat src_st = {0};
  std::string dst_tmppath;
  struct sigaction old_act;
  struct sigaction new_act;

  // Done in case fcntl(F_SETLEASE) works.
  new_act.sa_handler = SIG_IGN;
  new_act.sa_flags = 0;
  sigemptyset(&new_act.sa_mask);
  sigaction(SIGIO,&new_act,&old_act);
  DEFER { sigaction(SIGIO,&old_act,NULL); };

  while(true)
    {
      // For comparison after the copy to see if the file was
      // modified. This could be made more thorough by adding some
      // hashing but probably overkill. Also used to provide size and
      // other details for data copying.
      rv = fs::fstat(src_fd_,&src_st);
      if(rv < 0)
        return rv;
      if(!S_ISREG(src_st.st_mode))
        return -EINVAL;

      std::tie(dst_fd,dst_tmppath) = fs::mktemp(dst_filepath_,O_RDWR);
      if(dst_fd < 0)
        return dst_fd;
      DEFER { fs::close(dst_fd); };

      // If it fails or is unsupported... so be it.  This will help
      // limit the possibility of the file being modified while the
      // copy happens. Opening read-only is fine but open for write or
      // truncate will block for others till this finishes or the
      // kernel wide timeout (/proc/sys/fs/lease-break-time).
      fs::fcntl_setlease_rdlck(src_fd_);
      DEFER { fs::fcntl_setlease_unlck(src_fd_); };

      rv = fs::copyfile(src_fd_,src_st,dst_fd);
      if(rv < 0)
        {
          if(flags_.cleanup_failure)
            fs::unlink(dst_tmppath);
          return rv;
        }

      rv = fs::file_changed(src_fd_,src_st);
      if(rv == FS_FILE_CHANGED)
        {
          fs::unlink(dst_tmppath);
          continue;
        }

      rv = fs::rename(dst_tmppath,dst_filepath_);
      if((rv < 0) && (flags_.cleanup_failure))
        fs::unlink(dst_tmppath);
      break;
    }

  return rv;
}

s64
fs::copyfile(const fs::path          &src_filepath_,
             const fs::path          &dst_filepath_,
             const fs::CopyFileFlags &flags_)
{
  int src_fd;

  src_fd = fs::open(src_filepath_,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_NOATIME);
  if(src_fd < 0)
    return src_fd;
  DEFER { fs::close(src_fd); };

  return fs::copyfile(src_fd,dst_filepath_,flags_);
}

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

#include "fs_futimesat.hpp"
#include "fs_stat_utils.hpp"

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef UTIME_NOW
# define UTIME_NOW  ((1l << 30) - 1l)
#endif

#ifndef UTIME_OMIT
# define UTIME_OMIT ((1l << 30) - 2l)
#endif


namespace l
{
  static
  inline
  bool
  can_call_lutimes(const int          dirfd_,
                   const std::string &path_,
                   const int          flags_)
  {
    return ((flags_ == AT_SYMLINK_NOFOLLOW) &&
            ((dirfd_ == AT_FDCWD) ||
             (path_[0] == '/')));
  }

  static
  inline
  bool
  should_ignore(const struct timespec ts_[2])
  {
    return ((ts_ != NULL) &&
            (ts_[0].tv_nsec == UTIME_OMIT) &&
            (ts_[1].tv_nsec == UTIME_OMIT));
  }

  static
  inline
  bool
  should_be_set_to_now(const struct timespec ts_[2])
  {
    return ((ts_ == NULL) ||
            ((ts_[0].tv_nsec == UTIME_NOW) &&
             (ts_[1].tv_nsec == UTIME_NOW)));
  }

  static
  inline
  bool
  timespec_invalid(const struct timespec &ts_)
  {
    return (((ts_.tv_nsec < 0) ||
             (ts_.tv_nsec > 999999999)) &&
            ((ts_.tv_nsec != UTIME_NOW) &&
             (ts_.tv_nsec != UTIME_OMIT)));
  }

  static
  inline
  bool
  timespec_invalid(const struct timespec ts_[2])
  {
    return ((ts_ != NULL) &&
            (l::timespec_invalid(ts_[0]) ||
             l::timespec_invalid(ts_[1])));
  }

  static
  inline
  bool
  flags_invalid(const int flags_)
  {
    return ((flags_ & ~AT_SYMLINK_NOFOLLOW) != 0);
  }

  static
  inline
  bool
  any_timespec_is_utime_omit(const struct timespec ts_[2])
  {
    return ((ts_[0].tv_nsec == UTIME_OMIT) ||
            (ts_[1].tv_nsec == UTIME_OMIT));
  }

  static
  inline
  bool
  any_timespec_is_utime_now(const struct timespec ts_[2])
  {
    return ((ts_[0].tv_nsec == UTIME_NOW) ||
            (ts_[1].tv_nsec == UTIME_NOW));
  }

  static
  inline
  int
  set_utime_omit_to_current_value(const int              dirfd_,
                                  const std::string     &path_,
                                  const struct timespec  ts_[2],
                                  struct timeval         tv_[2],
                                  const int              flags_)
  {
    int rv;
    struct stat st;
    timespec *atime;
    timespec *mtime;

    if(!l::any_timespec_is_utime_omit(ts_))
      return 0;

    rv = fs::fstatat(dirfd_,path_,&st,flags_);
    if(rv == -1)
      return -1;

    atime = fs::stat_atime(st);
    mtime = fs::stat_mtime(st);

    if(ts_[0].tv_nsec == UTIME_OMIT)
      TIMESPEC_TO_TIMEVAL(&tv[0],atime);
    if(ts_[1].tv_nsec == UTIME_OMIT)
      TIMESPEC_TO_TIMEVAL(&tv[1],mtime);

    return 0;
  }

  static
  inline
  int
  set_utime_omit_to_current_value(const int             fd_,
                                  const struct timespec ts_[2],
                                  struct timeval        tv_[2])
  {
    int rv;
    struct stat st;
    timespec *atime;
    timespec *mtime;

    if(!l::any_timespec_is_utime_omit(ts_))
      return 0;

    rv = fs::fstat(fd_,&st);
    if(rv == -1)
      return -1;

    atime = fs::stat_atime(st);
    mtime = fs::stat_mtime(st);

    if(ts_[0].tv_nsec == UTIME_OMIT)
      TIMESPEC_TO_TIMEVAL(&tv_[0],atime);
    if(ts_[1].tv_nsec == UTIME_OMIT)
      TIMESPEC_TO_TIMEVAL(&tv_[1],mtime);

    return 0;
  }

  static
  inline
  int
  set_utime_now_to_now(const struct timespec ts_[2],
                       struct timeval        tv_[2])
  {
    int rv;
    struct timeval now;

    if(l::any_timespec_is_utime_now(ts_))
      return 0;

    rv = time::gettimeofday(&now,NULL);
    if(rv == -1)
      return -1;

    if(ts_[0].tv_nsec == UTIME_NOW)
      tv_[0] = now;
    if(ts_[1].tv_nsec == UTIME_NOW)
      tv_[1] = now;

    return 0;
  }

  static
  inline
  int
  convert_timespec_to_timeval(const int               dirfd_,
                              const std::string      &path_,
                              const struct timespec   ts_[2],
                              struct timeval          tv_[2],
                              struct timeval        **tvp_,
                              const int               flags_)
  {
    int rv;

    if(l::should_be_set_to_now(ts_))
      return (tvp=NULL,0);

    TIMESPEC_TO_TIMEVAL(&tv_[0],&ts_[0]);
    TIMESPEC_TO_TIMEVAL(&tv_[1],&ts_[1]);

    rv = l::set_utime_omit_to_current_value(dirfd_,path_,ts_,tv_,flags_);
    if(rv == -1)
      return -1;

    rv = l::set_utime_now_to_now(ts_,tv_);
    if(rv == -1)
      return -1;

    return (*tvp_=tv_,0);
  }

  static
  inline
  int
  convert_timespec_to_timeval(const int               fd_,
                              const struct timespec   ts_[2],
                              struct timeval          tv_[2],
                              struct timeval        **tvp_)
  {
    int rv;

    if(l::should_be_set_to_now(ts_))
      return (*tvp=NULL,0);

    TIMESPEC_TO_TIMEVAL(&tv_[0],&ts_[0]);
    TIMESPEC_TO_TIMEVAL(&tv_[1],&ts_[1]);

    rv = l::set_utime_omit_to_current_value(fd_,ts_,tv_);
    if(rv == -1)
      return -1;

    rv = l::set_utime_now_to_now(ts_,tv_);
    if(rv == -1)
      return -1;

    return (*tvp=tv,0);
  }
}

namespace fs
{
  static
  inline
  int
  futimens(const int             fd_,
           const struct timespec ts_[2])
  {
    int rv;
    struct timeval  tv[2];
    struct timeval *tvp;

    if(l::timespec_invalid(ts_))
      return (errno=EINVAL,-1);
    if(l::should_ignore(ts_))
      return 0;

    rv = l::convert_timespec_to_timeval(fd_,ts_,tv,&tvp);
    if(rv == -1)
      return -1;

    return ::futimes(fd_,tvp);
  }
}

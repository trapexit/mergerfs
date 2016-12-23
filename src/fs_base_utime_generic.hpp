/*
  ISC License

  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#ifndef __FS_BASE_UTIME_GENERIC_HPP__
#define __FS_BASE_UTIME_GENERIC_HPP__

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

static
inline
bool
_can_call_lutimes(const int          dirfd,
                  const std::string &path,
                  const int          flags)
{
  return ((flags == AT_SYMLINK_NOFOLLOW) &&
          ((dirfd == AT_FDCWD) ||
           (path[0] == '/')));
}

static
inline
bool
_should_ignore(const struct timespec ts[2])
{
  return ((ts != NULL) &&
          (ts[0].tv_nsec == UTIME_OMIT) &&
          (ts[1].tv_nsec == UTIME_OMIT));
}

static
inline
bool
_should_be_set_to_now(const struct timespec ts[2])
{
  return ((ts == NULL) ||
          ((ts[0].tv_nsec == UTIME_NOW) &&
           (ts[1].tv_nsec == UTIME_NOW)));
}

static
inline
bool
_timespec_invalid(const struct timespec &ts)
{
  return (((ts.tv_nsec < 0) ||
           (ts.tv_nsec > 999999999)) &&
          ((ts.tv_nsec != UTIME_NOW) &&
           (ts.tv_nsec != UTIME_OMIT)));
}

static
inline
bool
_timespec_invalid(const struct timespec ts[2])
{
  return ((ts != NULL) &&
          (_timespec_invalid(ts[0]) ||
           _timespec_invalid(ts[1])));
}

static
inline
bool
_flags_invalid(const int flags)
{
  return ((flags & ~AT_SYMLINK_NOFOLLOW) != 0);
}

static
inline
bool
_any_timespec_is_utime_omit(const struct timespec ts[2])
{
  return ((ts[0].tv_nsec == UTIME_OMIT) ||
          (ts[1].tv_nsec == UTIME_OMIT));
}

static
inline
bool
_any_timespec_is_utime_now(const struct timespec ts[2])
{
  return ((ts[0].tv_nsec == UTIME_NOW) ||
          (ts[1].tv_nsec == UTIME_NOW));
}

static
inline
int
_set_utime_omit_to_current_value(const int              dirfd,
                                 const std::string     &path,
                                 const struct timespec  ts[2],
                                 struct timeval         tv[2],
                                 const int              flags)
{
  int rv;
  struct stat st;

  if(!_any_timespec_is_utime_omit(ts))
    return 0;

  rv = ::fstatat(dirfd,path.c_str(),&st,flags);
  if(rv == -1)
    return -1;

  if(ts[0].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv[0],&st.st_atim);
  if(ts[1].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv[1],&st.st_mtim);

  return 0;
}

static
inline
int
_set_utime_omit_to_current_value(const int             fd,
                                 const struct timespec ts[2],
                                 struct timeval        tv[2])
{
  int rv;
  struct stat st;

  if(!_any_timespec_is_utime_omit(ts))
    return 0;

  rv = ::fstat(fd,&st);
  if(rv == -1)
    return -1;

  if(ts[0].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv[0],&st.st_atim);
  if(ts[1].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv[1],&st.st_mtim);

  return 0;
}

static
inline
int
_set_utime_now_to_now(const struct timespec ts[2],
                      struct timeval        tv[2])
{
  int rv;
  struct timeval now;

  if(_any_timespec_is_utime_now(ts))
    return 0;

  rv = ::gettimeofday(&now,NULL);
  if(rv == -1)
    return -1;

  if(ts[0].tv_nsec == UTIME_NOW)
    tv[0] = now;
  if(ts[1].tv_nsec == UTIME_NOW)
    tv[1] = now;

  return 0;
}

static
inline
int
_convert_timespec_to_timeval(const int               dirfd,
                             const std::string      &path,
                             const struct timespec   ts[2],
                             struct timeval          tv[2],
                             struct timeval        *&tvp,
                             const int               flags)
{
  int rv;

  if(_should_be_set_to_now(ts))
    return (tvp=NULL,0);

  TIMESPEC_TO_TIMEVAL(&tv[0],&ts[0]);
  TIMESPEC_TO_TIMEVAL(&tv[1],&ts[1]);

  rv = _set_utime_omit_to_current_value(dirfd,path,ts,tv,flags);
  if(rv == -1)
    return -1;

  rv = _set_utime_now_to_now(ts,tv);
  if(rv == -1)
    return -1;

  return (tvp=tv,0);
}

static
inline
int
_convert_timespec_to_timeval(const int               fd,
                             const struct timespec   ts[2],
                             struct timeval          tv[2],
                             struct timeval        *&tvp)
{
  int rv;

  if(_should_be_set_to_now(ts))
    return (tvp=NULL,0);

  TIMESPEC_TO_TIMEVAL(&tv[0],&ts[0]);
  TIMESPEC_TO_TIMEVAL(&tv[1],&ts[1]);

  rv = _set_utime_omit_to_current_value(fd,ts,tv);
  if(rv == -1)
    return -1;

  rv = _set_utime_now_to_now(ts,tv);
  if(rv == -1)
    return -1;

  return (tvp=tv,0);
}

namespace fs
{
  static
  inline
  int
  utime(const int              dirfd,
        const std::string     &path,
        const struct timespec  ts[2],
        const int              flags)
  {
    int rv;
    struct timeval  tv[2];
    struct timeval *tvp;

    if(_flags_invalid(flags))
      return (errno=EINVAL,-1);
    if(_timespec_invalid(ts))
      return (errno=EINVAL,-1);
    if(_should_ignore(ts))
      return 0;

    rv = _convert_timespec_to_timeval(dirfd,path,ts,tv,tvp,flags);
    if(rv == -1)
      return -1;

    if((flags & AT_SYMLINK_NOFOLLOW) == 0)
      return ::futimesat(dirfd,path.c_str(),tvp);
    if(_can_call_lutimes(dirfd,path,flags))
      return ::lutimes(path.c_str(),tvp);

    return (errno=ENOTSUP,-1);
  }

  static
  inline
  int
  utime(const int             fd,
        const struct timespec ts[2])
  {
    int rv;
    struct timeval  tv[2];
    struct timeval *tvp;

    if(_timespec_invalid(ts))
      return (errno=EINVAL,-1);
    if(_should_ignore(ts))
      return 0;

    rv = _convert_timespec_to_timeval(fd,ts,tv,tvp);
    if(rv == -1)
      return -1;

    return ::futimes(fd,tvp);
  }
}

#endif

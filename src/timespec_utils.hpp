#pragma once

static
inline
bool
operator<(const timespec &lhs_,
          const timespec &rhs_)
{
  if(lhs_.tv_sec != rhs_.tv_sec)
    return lhs_.tv_sec < rhs_.tv_sec;
  return lhs_.tv_nsec < rhs_.tv_nsec;
}

static
inline
bool
operator>(const timespec &lhs_,
          const timespec &rhs_)
{
  if(lhs_.tv_sec != rhs_.tv_sec)
    return lhs_.tv_sec > rhs_.tv_sec;
  return lhs_.tv_nsec > rhs_.tv_nsec;
}

static
inline
bool
operator==(const timespec &lhs_,
           const timespec &rhs_)
{
  return ((lhs_.tv_sec == rhs_.tv_sec) &&
          (lhs_.tv_nsec == rhs_.tv_nsec));
}

static
inline
bool
operator!=(const timespec &lhs_,
           const timespec &rhs_)
{
  return !(lhs_ == rhs_);
}

static
inline
bool
operator<=(const timespec &lhs_,
           const timespec &rhs_)
{
  return !(lhs_ > rhs_);
}

static
inline
bool
operator>=(const timespec &lhs_,
           const timespec &rhs_)
{
  return !(lhs_ < rhs_);
}

static
inline
bool
operator<(const fuse_sx_time &lhs_,
          const fuse_sx_time &rhs_)
{
  if(lhs_.tv_sec != rhs_.tv_sec)
    return lhs_.tv_sec < rhs_.tv_sec;
  return lhs_.tv_nsec < rhs_.tv_nsec;
}

static
inline
bool
operator>(const fuse_sx_time &lhs_,
          const fuse_sx_time &rhs_)
{
  if(lhs_.tv_sec != rhs_.tv_sec)
    return lhs_.tv_sec > rhs_.tv_sec;
  return lhs_.tv_nsec > rhs_.tv_nsec;
}

static
inline
bool
operator==(const fuse_sx_time &lhs_,
           const fuse_sx_time &rhs_)
{
  return ((lhs_.tv_sec == rhs_.tv_sec) &&
          (lhs_.tv_nsec == rhs_.tv_nsec));
}

static
inline
bool
operator!=(const fuse_sx_time &lhs_,
           const fuse_sx_time &rhs_)
{
  return !(lhs_ == rhs_);
}

static
inline
bool
operator<=(const fuse_sx_time &lhs_,
           const fuse_sx_time &rhs_)
{
  return !(lhs_ > rhs_);
}

static
inline
bool
operator>=(const fuse_sx_time &lhs_,
           const fuse_sx_time &rhs_)
{
  return !(lhs_ < rhs_);
}


namespace TimeSpec
{
  static
  inline
  timespec
  newest(const timespec &t0_,
         const timespec &t1_)
  {
    if(t0_.tv_sec > t1_.tv_sec)
      return t0_;
    if(t0_.tv_sec == t1_.tv_sec)
      {
        if(t0_.tv_nsec > t1_.tv_nsec)
          return t0_;
      }

    return t1_;
  }

  static
  inline
  fuse_sx_time
  newest(const fuse_sx_time &t0_,
         const fuse_sx_time &t1_)
  {
    if(t0_.tv_sec > t1_.tv_sec)
      return t0_;
    if(t0_.tv_sec == t1_.tv_sec)
      {
        if(t0_.tv_nsec > t1_.tv_nsec)
          return t0_;
      }

    return t1_;
  }
}

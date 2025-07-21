#pragma once

#include <errno.h>


template<typename T>
static
inline
T
to_neg_errno(const T   rv_,
             const int errno_)
{
  return ((rv_ == (T)-1) ? -errno_ : rv_);
}

template<typename T>
static
inline
T
to_neg_errno(const T rv_)
{
  return ::to_neg_errno(rv_,errno);
}

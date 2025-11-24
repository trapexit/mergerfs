#pragma once

#include "to_neg_errno.hpp"
#include "fs_path.hpp"

#include <fcntl.h>
#include <sys/stat.h>

namespace fs
{
  static
  inline
  int
  mkdirat(const int     dirfd_,
          const char   *pathname_,
          const mode_t  mode_)
  {
    int rv;

    rv = ::mkdirat(dirfd_,pathname_,mode_);

    return ::to_neg_errno(rv);
  }

  static
  inline
  int
  mkdirat(const int          dirfd_,
          const std::string &pathname_,
          const mode_t       mode_)
  {
    return fs::mkdirat(dirfd_,
                       pathname_.c_str(),
                       mode_);
  }

  static
  inline
  int
  mkdirat(const int       dirfd_,
          const fs::path &pathname_,
          const mode_t    mode_)
  {
    return fs::mkdirat(dirfd_,
                       pathname_.c_str(),
                       mode_);
  }
}

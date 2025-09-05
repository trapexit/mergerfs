#pragma once

#include "fs_lstat.hpp"

namespace thread_info
{
  static
  inline
  int
  process_count()
  {
    int rv;
    struct stat st;

    rv = fs::lstat("/proc/self/task",&st);
    if(rv < 0)
      return rv;

    return (st.st_nlink - 2);
  }
}

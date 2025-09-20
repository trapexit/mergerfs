#pragma once

#include <cstddef>
#include <cstdint>

namespace fs
{
  struct dirent64
  {
  public:
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];

  public:
    int d_namelen() const
    {
      return (d_reclen - offsetof(dirent64,d_name));
    }
  };
}

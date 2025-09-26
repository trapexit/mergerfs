#pragma once

#include <cstddef>
#include <cstdint>

namespace fs
{
  struct dirent64
  {
  public:
    uint64_t ino;
    int64_t  off;
    uint16_t reclen;
    uint8_t  type;
    char     name[];

  public:
    int namelen() const
    {
      return (reclen - offsetof(dirent64,name));
    }
  };
}

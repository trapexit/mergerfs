#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

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
    size_t
    namelen() const
    {
      return ::strlen(name);
    }
  };
}

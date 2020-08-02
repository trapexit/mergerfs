#pragma once

#include <stdint.h>

struct linux_dirent64
{
  uint64_t ino;
  int64_t  off;
  uint16_t reclen;
  uint8_t  type;
  char     name[];
};

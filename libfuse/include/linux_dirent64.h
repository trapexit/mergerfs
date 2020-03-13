#pragma once

#include <stdint.h>

struct linux_dirent64
{
  uint64_t d_ino;
  int64_t  d_off;
  uint16_t d_reclen;
  uint8_t  d_type;
  char     d_name[];
};

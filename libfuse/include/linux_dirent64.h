#pragma once

#include <stdint.h>

#define DIRENT_NAMELEN(X) ((X)->reclen - offsetof(linux_dirent64_t,name))

typedef struct linux_dirent64_t linux_dirent64_t;
struct linux_dirent64_t
{
  uint64_t ino;
  int64_t  off;
  uint16_t reclen;
  uint8_t  type;
  char     name[];
};

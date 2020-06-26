#pragma once

struct linux_dirent
{
  unsigned long  ino;
  unsigned long  off;
  unsigned short reclen;
  char           name[];
};

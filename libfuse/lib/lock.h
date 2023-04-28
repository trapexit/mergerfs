#pragma once

struct lock
{
  int type;
  off_t start;
  off_t end;
  pid_t pid;
  uint64_t owner;
  struct lock *next;
};

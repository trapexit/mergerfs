#pragma once

#include <sys/types.h>


namespace ioprio
{
  int get(const int which, const int who);
  int set(const int which, const int who, const int ioprio);

  struct SetFrom
  {
    static thread_local int prio;
    SetFrom(const pid_t pid);
  };
};

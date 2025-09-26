#pragma once

#include <sys/types.h>


namespace ioprio
{
  void enable(const bool);
  bool enabled();

  int get(const int which, const int who);
  int set(const int which, const int who, const int ioprio);

  struct SetFrom
  {
    static thread_local int thread_prio;

    SetFrom(const pid_t pid);
  };
};

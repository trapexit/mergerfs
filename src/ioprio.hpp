#pragma once

namespace ioprio
{
  int get(const int which, const int who);
  int set(const int which, const int who, const int ioprio);

  struct SetFrom
  {
    SetFrom(const pid_t pid);
  };
};

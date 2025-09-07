#pragma once

#include "fs_path.hpp"

#include <vector>


namespace fs
{
  struct Mount
  {
    fs::path    dir;
    std::string fsname;
    std::string type;
    std::string opts;
  };

  typedef std::vector<fs::Mount> MountVec;

  void mounts(fs::MountVec &mounts);
}

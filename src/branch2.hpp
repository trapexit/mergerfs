#pragma once

#include "ghc/filesystem.hpp"
#include "toml.hpp"

#include "enum.h"

#include <stdint.h>

BETTER_ENUM(BranchMode,int,RO,RW,NC);

class Branch2
{
public:
  //  typedef BranchMode Mode;

public:
  Branch2(toml::value const &);
  
public:
  bool enabled;
  Mode mode;
  uint64_t min_free_space;
  int fd;
  ghc::filesystem::path path;
};

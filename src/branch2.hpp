#pragma once

#include "ghc/filesystem.hpp"
#include "toml.hpp"

#include "enum.h"

#include <stdint.h>

BETTER_ENUM(BranchMode,int,RO,RW,NC);

class Branch2
{
public:
  typedef BranchMode Mode;

public:
  Branch2();
  Branch2(Branch2 const &);
  Branch2(Branch2 &&);
  Branch2(toml::value const &value,
          uint64_t const     min_free_space = 0);
  ~Branch2();
  Branch2& operator=(Branch2 const &);

public:
  bool matches_exclude(ghc::filesystem::path const &path) const;

public:
  bool enabled;
  Mode mode;
  uint64_t min_free_space;
  int fd;
  ghc::filesystem::path path;
  std::vector<std::string> excludes;
};

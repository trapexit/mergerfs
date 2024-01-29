#pragma once

#include "branch2.hpp"

#include "toml.hpp"

#include <vector>

class BranchTier
{
public:
  BranchTier();
  BranchTier(toml::value const &);

public:
  uint64_t min_free_space;

private:
  std::vector<Branch2>  _branches;
};

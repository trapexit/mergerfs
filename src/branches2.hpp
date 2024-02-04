#pragma once

#include "branch2.hpp"
#include "branch_tier.hpp"

#include "toml.hpp"

#include <vector>

class Branches2
{
public:
  Branches2();
  Branches2(toml::value const &);

public:
  uint64_t min_free_space;

public:


public:
  std::vector<BranchTier>::iterator begin() { return _branches.begin(); }
  std::vector<BranchTier>::iterator end() { return _branches.end(); }

private:
  std::vector<BranchTier> _branches;
};

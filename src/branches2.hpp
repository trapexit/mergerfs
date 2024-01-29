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

private:
  std::vector<Branch2Tier> _branches;
};

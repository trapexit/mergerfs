#include "branch2.hpp"

#include "toml.hpp"

#include <vector>

class Branch2Group
{
public:
  uint64_t min_free_space;

private:
  std::vector<Branch2>  _branches;
};

class Branches2
{
public:
  Branches2();
  Branches2(toml::value const &);

public:
  uint64_t min_free_space;

private:
  std::vector<Branch2Group> _branches;
};

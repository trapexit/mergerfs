#include "branch2.hpp"

#include <vector>

typedef std::vector<Branch2> Branch2Vec;


class Branches2 : public std::vector<Branch2Vec>
{
  uint64_t min_free_space;
};

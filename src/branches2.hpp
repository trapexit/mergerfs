#include "branch2.hpp"

#include <vector>

class Branch2Vec : public std::vector<Branch2>
{

};

class Branches2 : public std::vector<Branch2Vec>
{
public:
  uint64_t min_free_space;
};

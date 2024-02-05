#include "branches2.hpp"
#include "func_create.hpp"

#include <memory>

class State
{
public:
  State();

public:
  Branches2 branches;

public:
  std::unique_ptr<Func::CreateBase> create;
};

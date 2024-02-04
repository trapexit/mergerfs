#include "branches2.hpp"
#include "func_create_base.hpp"


class State
{
public:
  State();
  
public:
  Branches2 branches;

public:
  std::unique_ptr<Func::CreateBase> create;
};

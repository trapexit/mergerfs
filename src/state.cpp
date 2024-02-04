#include "state.hpp"

#include "func_create_ff.hpp"

#include "make_unique.hpp"

State state;

State::State()
{
  create = std::make_unique<Func::CreateFF>();
}


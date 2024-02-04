#include "state.hpp"

#include "make_unique.hpp"

State state;

State::State()
{
  create = std::make_unique<Func::CreateFF>();
}


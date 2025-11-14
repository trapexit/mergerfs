#include "func_open_factory.hpp"

#include "func_open_ff.hpp"

#include "policies.hpp"


bool
Func2::OpenFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::OpenBase>
Func2::OpenFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::OpenFF>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::OpenFF>();

  return {};
}

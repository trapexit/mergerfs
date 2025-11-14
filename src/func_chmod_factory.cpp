#include "func_chmod_factory.hpp"

#include "func_chmod_all.hpp"

#include "policies.hpp"


bool
Func2::ChmodFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::ChmodBase>
Func2::ChmodFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::ChmodAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::ChmodAll>();

  return {};
}

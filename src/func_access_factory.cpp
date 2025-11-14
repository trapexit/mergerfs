#include "func_access_factory.hpp"

#include "func_access_all.hpp"

#include "policies.hpp"


bool
Func2::AccessFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::AccessBase>
Func2::AccessFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::AccessAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::AccessAll>();

  return {};
}

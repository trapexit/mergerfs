#include "func_chown_factory.hpp"

#include "func_chown_all.hpp"

#include "policies.hpp"


bool
Func2::ChownFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::ChownBase>
Func2::ChownFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::ChownAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::ChownAll>();

  return {};
}

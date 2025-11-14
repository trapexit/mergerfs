#include "func_rmdir_factory.hpp"

#include "func_rmdir_all.hpp"

#include "policies.hpp"


bool
Func2::RmdirFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::RmdirBase>
Func2::RmdirFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::RmdirAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::RmdirAll>();

  return {};
}

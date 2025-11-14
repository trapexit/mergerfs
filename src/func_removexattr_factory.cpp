#include "func_removexattr_factory.hpp"

#include "func_removexattr_all.hpp"

#include "policies.hpp"


bool
Func2::RemovexattrFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::RemovexattrBase>
Func2::RemovexattrFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::RemovexattrAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::RemovexattrAll>();

  return {};
}

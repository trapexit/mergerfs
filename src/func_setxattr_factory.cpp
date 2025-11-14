#include "func_setxattr_factory.hpp"

#include "func_setxattr_all.hpp"

#include "policies.hpp"


bool
Func2::SetxattrFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::SetxattrBase>
Func2::SetxattrFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::SetxattrAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::SetxattrAll>();

  return {};
}

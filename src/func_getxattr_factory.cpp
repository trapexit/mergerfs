#include "func_getxattr_factory.hpp"

#include "func_getxattr_ff.hpp"

#include "policies.hpp"


bool
Func2::GetxattrFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::GetxattrBase>
Func2::GetxattrFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::GetxattrFF>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::GetxattrFF>();

  return {};
}

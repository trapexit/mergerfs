#include "func_ioctl_factory.hpp"

#include "func_ioctl_ff.hpp"

#include "policies.hpp"


bool
Func2::IoctlFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::IoctlBase>
Func2::IoctlFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::IoctlFF>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::IoctlFF>();

  return {};
}

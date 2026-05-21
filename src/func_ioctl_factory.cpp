#include "func_ioctl_factory.hpp"

#include "func_ioctl_ff.hpp"
#include "func_ioctl_erofs.hpp"


bool
Func2::IoctlFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::IoctlBase>
Func2::IoctlFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::IoctlEROFS>();
  if(str_ == "ff")    return std::make_shared<Func2::IoctlFF>();

  return {};
}

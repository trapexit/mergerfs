#include "func_readlink_factory.hpp"

#include "func_readlink_ff.hpp"
#include "func_readlink_erofs.hpp"


bool
Func2::ReadlinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ReadlinkBase>
Func2::ReadlinkFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::ReadlinkEROFS>();
  if(str_ == "ff")    return std::make_shared<Func2::ReadlinkFF>();

  return {};
}

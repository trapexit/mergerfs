#include "func_open_factory.hpp"

#include "func_open_ff.hpp"
#include "func_open_erofs.hpp"


bool
Func2::OpenFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::OpenBase>
Func2::OpenFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::OpenEROFS>();
  if(str_ == "ff")    return std::make_shared<Func2::OpenFF>();

  return {};
}

#include "func_access_factory.hpp"

#include "func_access_all.hpp"
#include "func_access_epall.hpp"
#include "func_access_erofs.hpp"
#include "func_access_ff.hpp"


bool
Func2::AccessFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::AccessBase>
Func2::AccessFactory::make(const std::string_view str_)
{
  if(str_ == "all")    return std::make_shared<Func2::AccessAll>();
  if(str_ == "epall")  return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "erofs")  return std::make_shared<Func2::AccessEROFS>();
  if(str_ == "ff")     return std::make_shared<Func2::AccessFF>();

  return {};
}

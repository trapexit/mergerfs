#include "func_unlink_factory.hpp"

#include "func_unlink_all.hpp"
#include "func_unlink_erofs.hpp"
#include "func_unlink_newest.hpp"


bool
Func2::UnlinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::UnlinkBase>
Func2::UnlinkFactory::make(const std::string_view str_)
{
  if(str_ == "all")    return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "epall")  return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "erofs")  return std::make_shared<Func2::UnlinkEROFS>();
  if(str_ == "newest") return std::make_shared<Func2::UnlinkNewest>();

  return {};
}

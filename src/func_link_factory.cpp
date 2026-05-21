#include "func_link_factory.hpp"

#include "func_link_epall.hpp"
#include "func_link_erofs.hpp"


bool
Func2::LinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::LinkBase>
Func2::LinkFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")               return std::make_shared<Func2::LinkEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::LinkEPAll>();

  return {};
}

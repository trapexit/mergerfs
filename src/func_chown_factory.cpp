#include "func_chown_factory.hpp"

#include "func_chown_all.hpp"
#include "func_chown_erofs.hpp"


bool
Func2::ChownFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ChownBase>
Func2::ChownFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")              return std::make_shared<Func2::ChownEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::ChownAll>();

  return {};
}

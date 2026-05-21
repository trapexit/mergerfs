#include "func_chmod_factory.hpp"

#include "func_chmod_all.hpp"
#include "func_chmod_erofs.hpp"


bool
Func2::ChmodFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ChmodBase>
Func2::ChmodFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")              return std::make_shared<Func2::ChmodEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::ChmodAll>();

  return {};
}

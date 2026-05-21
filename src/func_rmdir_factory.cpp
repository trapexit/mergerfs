#include "func_rmdir_factory.hpp"

#include "func_rmdir_all.hpp"
#include "func_rmdir_erofs.hpp"


bool
Func2::RmdirFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::RmdirBase>
Func2::RmdirFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")              return std::make_shared<Func2::RmdirEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::RmdirAll>();

  return {};
}

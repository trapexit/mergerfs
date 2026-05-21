#include "func_rename_factory.hpp"

#include "func_rename_epall.hpp"
#include "func_rename_erofs.hpp"


bool
Func2::RenameFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::RenameBase>
Func2::RenameFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")               return std::make_shared<Func2::RenameEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::RenameEPAll>();

  return {};
}

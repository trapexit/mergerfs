#include "func_removexattr_factory.hpp"

#include "func_removexattr_all.hpp"
#include "func_removexattr_erofs.hpp"


bool
Func2::RemovexattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::RemovexattrBase>
Func2::RemovexattrFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")              return std::make_shared<Func2::RemovexattrEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::RemovexattrAll>();

  return {};
}

#include "func_setxattr_factory.hpp"

#include "func_setxattr_all.hpp"
#include "func_setxattr_erofs.hpp"


bool
Func2::SetxattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::SetxattrBase>
Func2::SetxattrFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")              return std::make_shared<Func2::SetxattrEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::SetxattrAll>();

  return {};
}

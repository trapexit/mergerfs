#include "func_getxattr_factory.hpp"

#include "func_getxattr_ff.hpp"
#include "func_getxattr_erofs.hpp"


bool
Func2::GetxattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::GetxattrBase>
Func2::GetxattrFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::GetxattrEROFS>();
  if(str_ == "ff")    return std::make_shared<Func2::GetxattrFF>();

  return {};
}

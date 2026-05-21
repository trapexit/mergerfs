#include "func_listxattr_factory.hpp"

#include "func_listxattr_all.hpp"
#include "func_listxattr_erofs.hpp"
#include "func_listxattr_ff.hpp"


bool
Func2::ListxattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ListxattrBase>
Func2::ListxattrFactory::make(const std::string_view str_)
{
  if(str_ == "all")   return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "epall") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "erofs") return std::make_shared<Func2::ListxattrEROFS>();
  if(str_ == "ff")    return std::make_shared<Func2::ListxattrFF>();

  return {};
}

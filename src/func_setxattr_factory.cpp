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
  if(str_ == "erofs") return std::make_shared<Func2::SetxattrEROFS>();
  if(str_ == "all") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "epall") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "epff") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "eplus") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "eprand") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "ff") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "lfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "lup") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "lus") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "mfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "msplus") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "newest") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::SetxattrAll>();
  if(str_ == "rand") return std::make_shared<Func2::SetxattrAll>();

  return {};
}

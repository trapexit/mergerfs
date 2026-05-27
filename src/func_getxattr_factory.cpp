#include "func_getxattr_factory.hpp"

#include "func_getxattr_erofs.hpp"
#include "func_getxattr_ff.hpp"


bool
Func2::GetxattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::GetxattrBase>
Func2::GetxattrFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::GetxattrEROFS>();
  if(str_ == "all") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "epall") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "epff") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "eplus") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "epmfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "eppfrd") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "eprand") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "ff") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "lfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "lup") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "lus") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "mfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "msplus") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "mspmfs") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "msppfrd") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "newest") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::GetxattrFF>();
  if(str_ == "rand") return std::make_shared<Func2::GetxattrFF>();

  return {};
}

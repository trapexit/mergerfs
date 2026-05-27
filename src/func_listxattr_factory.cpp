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
  if(str_ == "erofs") return std::make_shared<Func2::ListxattrEROFS>();
  if(str_ == "all") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "epall") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "epff") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "eplus") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "eprand") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "ff") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "lfs") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "lup") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "lus") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "mfs") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "msplus") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::ListxattrAll>();
  if(str_ == "newest") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::ListxattrFF>();
  if(str_ == "rand") return std::make_shared<Func2::ListxattrFF>();

  return {};
}

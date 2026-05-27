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
  if(str_ == "erofs") return std::make_shared<Func2::RmdirEROFS>();
  if(str_ == "all") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "epall") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "epff") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "eplus") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "eprand") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "ff") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "lfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "lup") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "lus") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "mfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "msplus") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "newest") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::RmdirAll>();
  if(str_ == "rand") return std::make_shared<Func2::RmdirAll>();

  return {};
}

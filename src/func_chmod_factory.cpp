#include "func_chmod_factory.hpp"

#include "func_chmod_all.hpp"
#include "func_chmod_erofs.hpp"


bool
Func2::ChmodFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ChmodBase>
Func2::ChmodFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::ChmodEROFS>();
  if(str_ == "all") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "epall") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "epff") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "eplus") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "eprand") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "ff") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "lfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "lup") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "lus") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "mfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "msplus") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "newest") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::ChmodAll>();
  if(str_ == "rand") return std::make_shared<Func2::ChmodAll>();

  return {};
}

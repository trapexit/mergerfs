#include "func_access_factory.hpp"

#include "func_access_all.hpp"
#include "func_access_epall.hpp"
#include "func_access_erofs.hpp"
#include "func_access_ff.hpp"


bool
Func2::AccessFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::AccessBase>
Func2::AccessFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::AccessEROFS>();
  if(str_ == "all") return std::make_shared<Func2::AccessAll>();
  if(str_ == "epall") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "epff") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "eplus") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "eprand") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "ff") return std::make_shared<Func2::AccessFF>();
  if(str_ == "lfs") return std::make_shared<Func2::AccessFF>();
  if(str_ == "lup") return std::make_shared<Func2::AccessFF>();
  if(str_ == "lus") return std::make_shared<Func2::AccessFF>();
  if(str_ == "mfs") return std::make_shared<Func2::AccessFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "msplus") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::AccessEPAll>();
  if(str_ == "newest") return std::make_shared<Func2::AccessFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::AccessFF>();
  if(str_ == "rand") return std::make_shared<Func2::AccessFF>();

  return {};
}

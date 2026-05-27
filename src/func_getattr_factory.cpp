#include "func_getattr_factory.hpp"

#include "func_getattr_cdco.hpp"
#include "func_getattr_cdfo.hpp"
#include "func_getattr_ff.hpp"
#include "func_getattr_newest.hpp"


bool
Func2::GetAttrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::GetAttrBase>
Func2::GetAttrFactory::make(const std::string_view str_)
{
  if(str_ == "cdfo")   return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "cdco")   return std::make_shared<Func2::GetAttrCDCO>();
  if(str_ == "ff")     return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "newest") return std::make_shared<Func2::GetAttrNewest>();
  if(str_ == "all") return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "epall") return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "epff") return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "eplus") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "epmfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "eppfrd") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "eprand") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "ff") return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "lfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "lup") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "lus") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "mfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "msplfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "msplus") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "mspmfs") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "msppfrd") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "newest") return std::make_shared<Func2::GetAttrNewest>();
  if(str_ == "pfrd") return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "rand") return std::make_shared<Func2::GetAttrCDFO>();

  return {};
}

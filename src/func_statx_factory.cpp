#include "func_statx_factory.hpp"

#include "func_statx_cdco.hpp"
#include "func_statx_cdfo.hpp"
#include "func_statx_ff.hpp"
#include "func_statx_newest.hpp"


bool
Func2::StatxFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::StatxBase>
Func2::StatxFactory::make(const std::string_view str_)
{
  if(str_ == "cdfo")   return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "cdco")   return std::make_shared<Func2::StatxCDCO>();
  if(str_ == "ff")     return std::make_shared<Func2::StatxFF>();
  if(str_ == "newest") return std::make_shared<Func2::StatxNewest>();
  if(str_ == "all") return std::make_shared<Func2::StatxFF>();
  if(str_ == "epall") return std::make_shared<Func2::StatxFF>();
  if(str_ == "epff") return std::make_shared<Func2::StatxFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "eplus") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "epmfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "eppfrd") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "eprand") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "ff") return std::make_shared<Func2::StatxFF>();
  if(str_ == "lfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "lup") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "lus") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "mfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "msplfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "msplus") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "mspmfs") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "msppfrd") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "newest") return std::make_shared<Func2::StatxNewest>();
  if(str_ == "pfrd") return std::make_shared<Func2::StatxCDFO>();
  if(str_ == "rand") return std::make_shared<Func2::StatxCDFO>();

  return {};
}

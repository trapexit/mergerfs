#include "func_statx_factory.hpp"

#include "func_statx_cdfo.hpp"
#include "func_statx_cdco.hpp"
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

  return {};
}

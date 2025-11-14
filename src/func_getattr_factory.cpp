#include "func_getattr_factory.hpp"

#include "func_getattr_cdfo.hpp"
#include "func_getattr_cdco.hpp"
#include "func_getattr_ff.hpp"
#include "func_getattr_newest.hpp"

#include "policies.hpp"

bool
Func2::GetAttrFactory::valid(const std::string str_)
{
  if(str_ == "cdfo")
    return true;
  if(str_ == "cdco")
    return true;
  if(str_ == "ff")
    return true;
  if(str_ == "newest")
    return true;

  if(Policies::Search::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::GetAttrBase>
Func2::GetAttrFactory::make(const std::string_view str_)
{
  if(str_ == "cdfo")
    return std::make_shared<Func2::GetAttrCDFO>();
  if(str_ == "cdco")
    return std::make_shared<Func2::GetAttrCDCO>();
  if(str_ == "ff")
    return std::make_shared<Func2::GetAttrFF>();
  if(str_ == "newest")
    return std::make_shared<Func2::GetAttrNewest>();

  if(Policies::Search::find(str_))
    return std::make_shared<Func2::GetAttrCDFO>();

  return {};
}

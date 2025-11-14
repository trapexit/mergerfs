#include "func_statx_factory.hpp"

#include "func_statx_combine.hpp"
#include "func_statx_ff.hpp"
#include "func_statx_newest.hpp"

#include "policies.hpp"

bool
Func2::StatxFactory::valid(const std::string str_)
{
  if(str_ == "combined")
    return true;
  if(str_ == "ff")
    return true;
  if(str_ == "newest")
    return true;

  if(Policies::Search::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::StatxBase>
Func2::StatxFactory::make(const std::string_view str_)
{
  if(str_ == "combine")
    return std::make_shared<Func2::StatxCombine>();
  if(str_ == "ff")
    return std::make_shared<Func2::StatxFF>();
  if(str_ == "newest")
    return std::make_shared<Func2::StatxNewest>();

  if(Policies::Search::find(str_))
    return std::make_shared<Func2::StatxCombine>();

  return {};
}

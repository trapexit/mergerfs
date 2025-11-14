#include "func_readlink_factory.hpp"

#include "func_readlink_ff.hpp"

#include "policies.hpp"


bool
Func2::ReadlinkFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;

  if(Policies::Search::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::ReadlinkBase>
Func2::ReadlinkFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::ReadlinkFF>();

  if(Policies::Search::find(str_))
    return std::make_shared<Func2::ReadlinkFF>();

  return {};
}

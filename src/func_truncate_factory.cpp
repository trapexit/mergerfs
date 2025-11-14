#include "func_truncate_factory.hpp"

#include "func_truncate_all.hpp"

#include "policies.hpp"


bool
Func2::TruncateFactory::valid(const std::string str_)
{
  if(str_ == "all")
    return true;

  if(Policies::Action::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::TruncateBase>
Func2::TruncateFactory::make(const std::string_view str_)
{
  if(str_ == "all")
    return std::make_shared<Func2::TruncateAll>();

  if(Policies::Action::find(str_))
    return std::make_shared<Func2::TruncateAll>();

  return {};
}

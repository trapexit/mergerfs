#include "func_listxattr_factory.hpp"

#include "func_listxattr_ff.hpp"

#include "policies.hpp"


bool
Func2::ListxattrFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;

  if(Policies::Search::find(str_))
    return true;

  return false;
}

std::shared_ptr<Func2::ListxattrBase>
Func2::ListxattrFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::ListxattrFF>();

  if(Policies::Search::find(str_))
    return std::make_shared<Func2::ListxattrFF>();

  return {};
}

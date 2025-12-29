#include "func_mknod_factory.hpp"

#include "func_mknod_ff.hpp"
#include "func_mknod_mfs.hpp"

bool
Func2::MknodFactory::valid(const std::string str_)
{
  if(str_ == "ff")
    return true;
  if(str_ == "mfs")
    return true;

  return false;
}

std::shared_ptr<Func2::MknodBase>
Func2::MknodFactory::make(const std::string_view str_)
{
  if(str_ == "ff")
    return std::make_shared<Func2::MknodFF>();
  if(str_ == "mfs")
    return std::make_shared<Func2::MknodMFS>();

  return {};
}
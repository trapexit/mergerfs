#include "func_unlink_factory.hpp"

#include "func_unlink_all.hpp"
#include "func_unlink_erofs.hpp"
#include "func_unlink_newest.hpp"


bool
Func2::UnlinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::UnlinkBase>
Func2::UnlinkFactory::make(const std::string_view str_)
{
  if(str_ == "erofs")  return std::make_shared<Func2::UnlinkEROFS>();
  if(str_ == "newest") return std::make_shared<Func2::UnlinkNewest>();
  if(str_ == "all") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "epall") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "epff") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "eplus") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "eprand") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "ff") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "lfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "lup") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "lus") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "mfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "msplus") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::UnlinkAll>();
  if(str_ == "rand") return std::make_shared<Func2::UnlinkAll>();

  return {};
}

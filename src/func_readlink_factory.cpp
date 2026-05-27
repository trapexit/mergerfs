#include "func_readlink_factory.hpp"

#include "func_readlink_erofs.hpp"
#include "func_readlink_ff.hpp"


bool
Func2::ReadlinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::ReadlinkBase>
Func2::ReadlinkFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::ReadlinkEROFS>();
  if(str_ == "all") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "epall") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "epff") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "eplus") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "epmfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "eppfrd") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "eprand") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "ff") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "lfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "lup") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "lus") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "mfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "msplus") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "mspmfs") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "msppfrd") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "newest") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::ReadlinkFF>();
  if(str_ == "rand") return std::make_shared<Func2::ReadlinkFF>();

  return {};
}

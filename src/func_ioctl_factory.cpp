#include "func_ioctl_factory.hpp"

#include "func_ioctl_erofs.hpp"
#include "func_ioctl_ff.hpp"


bool
Func2::IoctlFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::IoctlBase>
Func2::IoctlFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::IoctlEROFS>();
  if(str_ == "all") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "epall") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "epff") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "eplus") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "epmfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "eppfrd") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "eprand") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "ff") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "lfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "lup") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "lus") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "mfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "msplus") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "mspmfs") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "msppfrd") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "newest") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::IoctlFF>();
  if(str_ == "rand") return std::make_shared<Func2::IoctlFF>();

  return {};
}

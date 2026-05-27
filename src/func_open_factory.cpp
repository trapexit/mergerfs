#include "func_open_factory.hpp"

#include "func_open_erofs.hpp"
#include "func_open_ff.hpp"


bool
Func2::OpenFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::OpenBase>
Func2::OpenFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::OpenEROFS>();
  if(str_ == "all") return std::make_shared<Func2::OpenFF>();
  if(str_ == "epall") return std::make_shared<Func2::OpenFF>();
  if(str_ == "epff") return std::make_shared<Func2::OpenFF>();
  if(str_ == "eplfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "eplus") return std::make_shared<Func2::OpenFF>();
  if(str_ == "epmfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "eppfrd") return std::make_shared<Func2::OpenFF>();
  if(str_ == "eprand") return std::make_shared<Func2::OpenFF>();
  if(str_ == "ff") return std::make_shared<Func2::OpenFF>();
  if(str_ == "lfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "lup") return std::make_shared<Func2::OpenFF>();
  if(str_ == "lus") return std::make_shared<Func2::OpenFF>();
  if(str_ == "mfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "msplfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "msplus") return std::make_shared<Func2::OpenFF>();
  if(str_ == "mspmfs") return std::make_shared<Func2::OpenFF>();
  if(str_ == "msppfrd") return std::make_shared<Func2::OpenFF>();
  if(str_ == "newest") return std::make_shared<Func2::OpenFF>();
  if(str_ == "pfrd") return std::make_shared<Func2::OpenFF>();
  if(str_ == "rand") return std::make_shared<Func2::OpenFF>();

  return {};
}

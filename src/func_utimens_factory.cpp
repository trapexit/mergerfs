#include "func_utimens_factory.hpp"

#include "func_utimens_all.hpp"
#include "func_utimens_erofs.hpp"


bool
Func2::UtimensFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::UtimensBase>
Func2::UtimensFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::UtimensEROFS>();
  if(str_ == "all") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "epall") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "epff") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "eplus") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "eprand") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "ff") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "lfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "lup") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "lus") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "mfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "msplus") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "newest") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::UtimensAll>();
  if(str_ == "rand") return std::make_shared<Func2::UtimensAll>();

  return {};
}

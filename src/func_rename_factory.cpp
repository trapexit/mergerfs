#include "func_rename_factory.hpp"

#include "func_rename_epall.hpp"
#include "func_rename_erofs.hpp"


bool
Func2::RenameFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::RenameBase>
Func2::RenameFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::RenameEROFS>();
  if(str_ == "all") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "epall") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "epff") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "eplus") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "eprand") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "ff") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "lfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "lup") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "lus") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "mfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "msplus") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "newest") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::RenameEPAll>();
  if(str_ == "rand") return std::make_shared<Func2::RenameEPAll>();

  return {};
}

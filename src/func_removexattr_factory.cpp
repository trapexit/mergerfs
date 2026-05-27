#include "func_removexattr_factory.hpp"

#include "func_removexattr_all.hpp"
#include "func_removexattr_erofs.hpp"


bool
Func2::RemovexattrFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::RemovexattrBase>
Func2::RemovexattrFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::RemovexattrEROFS>();
  if(str_ == "all") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "epall") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "epff") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "eplus") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "eprand") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "ff") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "lfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "lup") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "lus") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "mfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "msplus") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "newest") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::RemovexattrAll>();
  if(str_ == "rand") return std::make_shared<Func2::RemovexattrAll>();

  return {};
}

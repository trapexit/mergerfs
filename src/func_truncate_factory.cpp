#include "func_truncate_factory.hpp"

#include "func_truncate_all.hpp"
#include "func_truncate_erofs.hpp"


bool
Func2::TruncateFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::TruncateBase>
Func2::TruncateFactory::make(const std::string_view str_)
{
  if(str_ == "erofs") return std::make_shared<Func2::TruncateEROFS>();
  if(str_ == "all") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "epall") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "epff") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "eplfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "eplus") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "epmfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "eppfrd") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "eprand") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "ff") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "lfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "lup") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "lus") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "mfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "msplfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "msplus") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "mspmfs") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "msppfrd") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "newest") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "pfrd") return std::make_shared<Func2::TruncateAll>();
  if(str_ == "rand") return std::make_shared<Func2::TruncateAll>();

  return {};
}

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
  if(str_ == "erofs")              return std::make_shared<Func2::TruncateEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::TruncateAll>();

  return {};
}

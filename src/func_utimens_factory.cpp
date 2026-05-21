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
  if(str_ == "erofs")              return std::make_shared<Func2::UtimensEROFS>();
  if(str_ == "all" || str_ == "epall") return std::make_shared<Func2::UtimensAll>();

  return {};
}

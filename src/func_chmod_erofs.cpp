#include "func_chmod_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::ChmodEROFS::name() const { return "erofs"; }

int
Func2::ChmodEROFS::operator()(const Branches&,const fs::path&,const mode_t)
{
  return -EROFS;
}

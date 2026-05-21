#include "func_rmdir_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::RmdirEROFS::name() const { return "erofs"; }

int
Func2::RmdirEROFS::operator()(const Branches&,const fs::path&)
{
  return -EROFS;
}

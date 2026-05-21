#include "func_utimens_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::UtimensEROFS::name() const { return "erofs"; }

int
Func2::UtimensEROFS::operator()(const Branches&,const fs::path&,const timespec[2])
{
  return -EROFS;
}

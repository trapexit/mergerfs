#include "func_unlink_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::UnlinkEROFS::name() const { return "erofs"; }

int
Func2::UnlinkEROFS::operator()(const Branches&,const fs::path&)
{
  return -EROFS;
}

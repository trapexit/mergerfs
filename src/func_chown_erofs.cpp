#include "func_chown_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::ChownEROFS::name() const { return "erofs"; }

int
Func2::ChownEROFS::operator()(const Branches&,const fs::path&,const uid_t,const gid_t)
{
  return -EROFS;
}

#include "func_removexattr_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::RemovexattrEROFS::name() const { return "erofs"; }

int
Func2::RemovexattrEROFS::operator()(const Branches&,const fs::path&,const char*)
{
  return -EROFS;
}

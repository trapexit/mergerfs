#include "func_setxattr_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::SetxattrEROFS::name() const { return "erofs"; }

int
Func2::SetxattrEROFS::operator()(const Branches&,const fs::path&,const char*,const char*,size_t,int)
{
  return -EROFS;
}

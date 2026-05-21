#include "func_getxattr_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::GetxattrEROFS::name() const { return "erofs"; }
int Func2::GetxattrEROFS::operator()(const Branches&,const fs::path&,const char*,char*,const size_t) { return -EROFS; }

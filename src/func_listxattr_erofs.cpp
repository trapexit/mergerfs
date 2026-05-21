#include "func_listxattr_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::ListxattrEROFS::name() const { return "erofs"; }
ssize_t Func2::ListxattrEROFS::operator()(const Branches&,const fs::path&,char*,const size_t) { return -EROFS; }

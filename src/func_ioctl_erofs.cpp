#include "func_ioctl_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::IoctlEROFS::name() const { return "erofs"; }
int Func2::IoctlEROFS::operator()(const Branches&,const fs::path&,const uint32_t&,void*&,uint32_t*&) { return -EROFS; }

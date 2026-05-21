#include "func_readlink_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::ReadlinkEROFS::name() const { return "erofs"; }
int Func2::ReadlinkEROFS::operator()(const Branches&,const fs::path&,char*,const size_t,const bool,const time_t) { return -EROFS; }

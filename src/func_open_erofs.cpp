#include "func_open_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::OpenEROFS::name() const { return "erofs"; }
int Func2::OpenEROFS::operator()(const Branches&,const fs::path&,fuse_file_info_t*&,const bool&,const NFSOpenHack&) { return -EROFS; }

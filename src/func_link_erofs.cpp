#include "func_link_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::LinkEROFS::name() const { return "erofs"; }
int Func2::LinkEROFS::operator()(const Branches&,const fs::path&,const fs::path&) { return -EROFS; }

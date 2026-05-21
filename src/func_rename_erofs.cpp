#include "func_rename_erofs.hpp"
#include "errno.hpp"
std::string_view Func2::RenameEROFS::name() const { return "erofs"; }
int Func2::RenameEROFS::operator()(const Branches&,const fs::path&,const fs::path&) { return -EROFS; }

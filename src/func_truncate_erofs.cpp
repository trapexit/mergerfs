#include "func_truncate_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::TruncateEROFS::name() const { return "erofs"; }

int
Func2::TruncateEROFS::operator()(const Branches&,const fs::path&,const off_t)
{
  return -EROFS;
}

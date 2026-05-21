#include "func_access_erofs.hpp"
#include "errno.hpp"

std::string_view Func2::AccessEROFS::name() const { return "erofs"; }

int
Func2::AccessEROFS::operator()(const Branches&,
                               const fs::path&,
                               const int)
{
  return -EROFS;
}

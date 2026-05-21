#include "func_create_erofs.hpp"

#include "errno.hpp"


std::string_view
Func2::CreateEROFS::name() const
{
  return "erofs";
}

bool
Func2::CreateEROFS::path_preserving() const
{
  return false;
}


int
Func2::CreateEROFS::operator()(const ugid_t      &ugid_,
                               const Branches    &branches_,
                               const fs::path    &fusepath_,
                               fuse_file_info_t *&ffi_,
                               const mode_t      &mode_,
                               const mode_t      &umask_)
{
  (void)ugid_;
  (void)branches_;
  (void)fusepath_;
  (void)ffi_;
  (void)mode_;
  (void)umask_;

  return -EROFS;
}

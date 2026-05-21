#include "func_mknod_erofs.hpp"

#include "errno.hpp"


std::string_view
Func2::MknodEROFS::name() const
{
  return "erofs";
}

int
Func2::MknodEROFS::operator()(const ugid_t   &ugid_,
                              const Branches &branches_,
                              const fs::path &fusepath_,
                              const mode_t    mode_,
                              const dev_t     dev_,
                              const mode_t    umask_)
{
  (void)ugid_;
  (void)branches_;
  (void)fusepath_;
  (void)mode_;
  (void)dev_;
  (void)umask_;

  return -EROFS;
}

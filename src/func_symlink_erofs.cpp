#include "func_symlink_erofs.hpp"

#include "errno.hpp"


std::string_view
Func2::SymlinkEROFS::name() const
{
  return "erofs";
}

int
Func2::SymlinkEROFS::operator()(const ugid_t    &ugid_,
                                const Branches  &branches_,
                                const char      *const &target_,
                                const fs::path  &linkpath_,
                                struct stat     *&st_)
{
  (void)ugid_;
  (void)branches_;
  (void)target_;
  (void)linkpath_;
  (void)st_;

  return -EROFS;
}

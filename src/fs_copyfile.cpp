#include "fs_copyfile.hpp"

#include "fs_clonefile.hpp"
#include "fs_close.hpp"
#include "fs_mktemp.hpp"
#include "fs_open.hpp"
#include "fs_rename.hpp"
#include "scope_guard.hpp"

#include <fcntl.h>


int
fs::copyfile(const std::filesystem::path &src_,
             const std::filesystem::path &dst_)
{
  int rv;
  int src_fd;
  int dst_fd;
  std::string dst_tmppath;

  src_fd = fs::open(src_,O_RDONLY|O_NOFOLLOW);
  if(src_fd < 0)
    return src_fd;
  DEFER { fs::close(src_fd); };

  std::tie(dst_fd,dst_tmppath) = fs::mktemp(dst_,O_RDWR);
  if(dst_fd < 0)
    return dst_fd;
  DEFER { fs::close(dst_fd); };

  rv = fs::clonefile(src_fd,dst_fd);
  if(rv < 0)
    return rv;

  rv = fs::rename(dst_tmppath,dst_);

  return rv;
}

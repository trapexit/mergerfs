#include "fsck_mergerfs.hpp"

#include "fs_lgetxattr.hpp"
#include "fs_ioctl.hpp"
#include "fs_open.hpp"
#include "fs_close.hpp"
#include "mergerfs_ioctl.hpp"
#include "str.hpp"

#include "fmt/core.h"
#include "CLI11.hpp"

#include <filesystem>

namespace FS = std::filesystem;

static
void
_compare_files(const std::vector<std::string> &paths_)
{
  struct stat st;

  if(paths_.size() <= 1)
    return;


}

static
void
_fsck(const FS::path &path_)
{
  IOCTL_BUF buf;
  std::vector<std::string> paths;

  auto opts = FS::directory_options::skip_permission_denied;
  auto rdi = FS::recursive_directory_iterator(path_,opts);
  for(const auto &de : rdi)
    {
      fmt::println("{}",de.path().string());

      strcpy(buf,"allpaths");

      int fd;
      int rv;

      fd = fs::open(de.path().string(),O_RDONLY|O_NOFOLLOW);
      if(fd == -1)
        continue;

      rv = fs::ioctl(fd,IOCTL_FILE_INFO,buf);
      if(rv != -1)
        {
          paths.clear();
          str::split(buf,'\0',&paths);
          ::_compare_files(paths);
        }

      fs::close(fd);
    }
}

int
fsck::main(int    argc_,
           char **argv_)
{
  CLI::App app{"fsck.mergerfs"};
  FS::path path;

  app.add_option("path",
                 path,
                 "mergerfs path");

  CLI11_PARSE(app,argc_,argv_);

  ::_fsck(path);

  return 0;
}

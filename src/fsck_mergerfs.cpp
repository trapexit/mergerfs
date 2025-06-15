#include "fsck_mergerfs.hpp"

#include "fs_close.hpp"
#include "fs_ioctl.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_lstat.hpp"
#include "fs_open.hpp"
#include "mergerfs_ioctl.hpp"
#include "str.hpp"
#include "int_types.h"

#include "fmt/core.h"
#include "CLI11.hpp"

#include <filesystem>

namespace FS = std::filesystem;

static
bool
_files_differ(const std::vector<struct stat> &stats_)
{
  for(u64 i = 1; i < stats_.size(); i++)
    {
      if(stats_[i].st_mode != stats_[0].st_mode)
        return true;
      if(stats_[i].st_uid != stats_[0].st_uid)
        return true;
      if(stats_[i].st_gid != stats_[0].st_gid)
        return true;
      if(stats_[i].st_size != stats_[0].st_size)
        return true;
    }

  return false;
}

static
void
_compare_files(const std::vector<std::string> &paths_)
{
  int rv;
  struct stat st;
  std::vector<struct stat> stats;

  if(paths_.size() <= 1)
    return;

  for(const auto &path : paths_)
    {
      st.st_size = -1;
      rv = fs::lstat(paths_[0],&st);
      stats.emplace_back(st);
    }

  for(u64 i = 1; i < paths_.size(); i++)
    {

    }
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

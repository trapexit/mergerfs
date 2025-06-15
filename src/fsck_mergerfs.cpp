#include "fsck_mergerfs.hpp"

#include "fs_close.hpp"
#include "fs_ioctl.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_lstat.hpp"
#include "fs_open.hpp"
#include "int_types.h"
#include "mergerfs_ioctl.hpp"
#include "str.hpp"

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
_compare_files(const std::string              &path_,
               const std::vector<std::string> &paths_)
{
  int rv;
  std::vector<struct stat> stats;

  if(paths_.size() <= 1)
    return;

  for(const auto &path : paths_)
    {
      struct stat st{};

      rv = fs::lstat(path,&st);
      if(rv == -1)
        st.st_size = -errno;
      stats.emplace_back(st);
    }

  if(::_files_differ(stats))
    {
      fmt::println("{}:",path_);
      for(u64 i = 0; i < paths_.size(); i++)
        {
          fmt::println(" * {}",paths_[i]);
          if(stats[i].st_size < 0)
            fmt::println("   err: {}",strerror(-stats[i].st_size));
          fmt::println("   uid: {}; gid: {}; mode: {}; size: {};",
                       stats[i].st_uid,
                       stats[i].st_gid,
                       stats[i].st_mode,
                       stats[i].st_size);
        }
    }
}

static
int
_get_allpaths(const std::string        &path_,
              std::vector<std::string> &paths_)
{
  int fd;
  int rv;
  IOCTL_BUF buf;

  strcpy(buf,"allpaths");

  fd = fs::open(de.path().string(),O_RDONLY|O_NOFOLLOW);
  if(fd == -1)
    continue;

  rv = fs::ioctl(fd,IOCTL_FILE_INFO,buf);

  fs::close(fd);

  str::split_on_null(buf,rv,&paths_);

  return rv;
}

static
void
_fsck(const FS::path &path_)
{
  int rv;
  std::vector<std::string> paths;

  auto opts = FS::directory_options::skip_permission_denied;
  auto rdi = FS::recursive_directory_iterator(path_,opts);
  for(const auto &de : rdi)
    {
      paths.clear();
      rv = ::_get_allpaths(de.path(),paths);
      if(rv == -1)
        continue;



      ::_compare_files(de.path(),paths);
    }
}

int
fsck::main(int    argc_,
           char **argv_)
{
  CLI::App app{"fsck.mergerfs"};
  FS::path path;

  app.add_option("path",path)
    ->description("mergerfs path")
    ->required();

  CLI11_PARSE(app,argc_,argv_);

  ::_fsck(path);

  return 0;
}
